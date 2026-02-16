#+#+#+#+---------------------------------------------------------------------
High-Performance UDP Send Path (ToyServer2)
Allocation-Free, Lifetime-Safe, Strand-Serialized
---------------------------------------------------------------------

Date: 2026-02-07

Target (main code locations)
- UDP session send + KCP output: `src/System/Session/UDPSession.cpp`
- UDP socket + receive path: `src/System/Network/UDPNetworkImpl.h`, `src/System/Network/UDPNetworkImpl.cpp`
- Packet pool + refcount: `src/System/Dispatcher/MessagePool.h`, `src/System/Dispatcher/MessagePool.cpp`, `src/System/Dispatcher/IMessage.h`
- Cross-check for Payload() assumptions: `src/System/Dispatcher/DISPATCHER/DispatcherImpl.cpp`, TCP sessions in `src/System/Session/*Session.cpp`

Goal
- Remove per-send heap allocation from UDP/KCP hot path.
- Eliminate raw-UDP payload memcpy into temporary buffers.
- Make async sends lifetime-safe (no use-after-free).
- Make UDP socket usage thread-safe under multi-IO-thread `io_context`.
- Keep existing `PacketMessage::Payload()` semantics intact (TCP/Dispatcher must not break).


=====================================================================
1) Executive Summary (TL;DR)
=====================================================================

ToyServer2 runs `boost::asio::io_context` on multiple IO threads (see `src/System/Framework/Framework.cpp`).
`UDPNetworkImpl` owns ONE UDP socket shared by all sessions.

Therefore, for a low-latency MMORPG server:

- Never build a temporary `std::vector<uint8_t>` per send.
- Never pass stack/thread_local buffers to `async_send_to`.
- Serialize ALL `async_send_to` initiations for the shared UDP socket using an Asio strand.
- Keep packet buffers alive until the completion handler runs.

Phase 1 (recommended) uses:
- scatter/gather send: [UDPTransportHeader bytes] + [PacketMessage::Payload bytes]
- a fixed `UDPSendContextPool` (no heap)
- a strand owned by `UDPNetworkImpl`

This fixes correctness first and removes the biggest performance killers.


=====================================================================
2) Ground Truth: Current Problems (Code-Verified)
=====================================================================

2.1 Hot path heap allocations (coding convention violation)
- `UDPSession` KCP output callback allocates/resizes `std::vector<uint8_t>` for every output frame.
- `UDPSession::Flush()` allocates/resizes `std::vector<uint8_t>` for every packet.

2.2 Redundant memcpy
- Raw UDP path copies `PacketMessage` payload -> vector.
- KCP path copies KCP buffer -> vector.

2.3 Async buffer lifetime bug
- `UDPNetworkImpl::SendTo(const uint8_t*, size_t, endpoint)` starts `async_send_to` but does not keep
  underlying memory alive until completion.
  Passing a local vector's `.data()` is undefined behavior.

2.4 Shared UDP socket thread-safety
- `UDPNetworkImpl` uses one socket for all sessions.
- With multiple IO threads, concurrent initiation or cross-thread access to the same socket is unsafe
  unless serialized.

2.5 Existing correctness bug: UDPSession `_isSending` can get stuck
- The base `Session::EnqueueSend` relies on derived `Flush()` to eventually allow future sends.
- Current `UDPSession::Flush()` drains the queue but never resets `_isSending=false`, which can stall
  future sends.


=====================================================================
3) Constraints and Invariants (Do Not Violate)
=====================================================================

3.1 Hot Path Rules (from `doc/coding_convention.md`)
- No heap allocation in send hot path.
- Avoid locks in IO hot path.
- Avoid unpredictable work.

3.2 `PacketMessage::Payload()` contract must remain
The codebase assumes Payload() points to the start of the packet bytes (typically `PacketHeader`).
Examples:
- Dispatcher reinterprets `Payload()` as `PacketHeader*`.
- TCP sessions memcpy packet bytes into `Payload()`.

Therefore:
- DO NOT change `MessagePool::AllocatePacket()` globally to add UDP headroom.
- DO NOT change `PacketMessage::Payload()` semantics.

3.3 `MessagePool::Free()` does not run derived destructors
In pooled path, `MessagePool::Free()` explicitly calls `msg->~IMessage()`.

Therefore:
- Do not store non-trivial members (e.g., `std::function`, `std::shared_ptr`, `PacketPtr`) in objects
  that are freed by `MessagePool::Free()` unless you redesign the pool.
- `UDPSendContext` must be trivially destructible.

3.4 Packed header + alignment
`UDPTransportHeader` is packed (alignment 1). Writing its fields directly may create unaligned stores.

Therefore:
- For sending, treat the UDP transport header as a byte blob and encode it using fixed offsets and
  `std::memcpy`.


=====================================================================
4) Datagram Size Policy (MMORPG / Ultra-Low-Latency Default)
=====================================================================

Goal: avoid IP fragmentation.

Recommended default:
- UDP_MAX_DATAGRAM_BYTES = 1200

Rationale (practical):
- Conservative payload size keeps us below common path MTUs and avoids fragmentation jitter/loss.
- Low-latency game traffic benefits more from stability than from large packets.

Make it easy to change:
- Define these constants in exactly one place (suggested: `src/System/Network/UDPLimits.h`).

Conceptual constants:

```cpp
static constexpr uint16_t UDP_MAX_DATAGRAM_BYTES = 1200; // bytes passed to async_send_to
static constexpr uint16_t UDP_TRANSPORT_HEADER_BYTES = (uint16_t)System::UDPTransportHeader::SIZE;
static constexpr uint16_t UDP_MAX_APP_BYTES = UDP_MAX_DATAGRAM_BYTES - UDP_TRANSPORT_HEADER_BYTES;
```

Enforcement rule:
- Raw UDP send must not send payloads larger than UDP_MAX_APP_BYTES.
  If exceeded: drop + rate-limited log + counter/metric increment.

KCP rule:
- KCP output carried over UDP must be configured so that its UDP payload size does not exceed
  UDP_MAX_APP_BYTES. If UDP_MAX_DATAGRAM_BYTES changes, revisit KCP MTU/segment sizing.


=====================================================================
5) Phase 1 Architecture (Correct, Minimal-Risk)
=====================================================================

Phase 1 is the plan we should implement first.
It fixes correctness and removes major allocations/copies without changing global packet layout.


---------------------------------------------------------------------
5.1 Core Idea
---------------------------------------------------------------------

Instead of building `[header][payload]` in a temporary vector, we send two buffers:

- Buffer 0: UDP transport header bytes (owned by a context object)
- Buffer 1: packet payload bytes (`PacketMessage::Payload()`) (owned by MessagePool)

Both must stay alive until `async_send_to` completion.


---------------------------------------------------------------------
5.2 UDPNetworkImpl owns a Strand (serialize shared socket)
---------------------------------------------------------------------

ToyServer2 runs `io_context` on N IO threads. The UDP socket is shared.
To make `async_send_to` initiation thread-safe, we serialize all send initiations using a strand
owned by `UDPNetworkImpl`.

Concrete implementation sketch (Boost.Asio):

```cpp
// UDPNetworkImpl.h
boost::asio::strand<boost::asio::any_io_executor> _strand;

// UDPNetworkImpl.cpp constructor
UDPNetworkImpl::UDPNetworkImpl(boost::asio::io_context& io)
    : _ioContext(io)
    , _socket(_ioContext)
    , _strand(boost::asio::make_strand(_socket.get_executor()))
{
}
```

Hard rule:
- Only call `_socket.async_send_to(...)` from code running on `_strand`.
- `UDPNetworkImpl::AsyncSend(...)` may be called from any thread; it must post to `_strand`.

TCP coexistence note:
- This strand is per UDP socket only. It does not serialize TCP sockets.
- TCP can continue to use IO thread parallelism; UDP sends are serialized only for their single
  shared socket.


---------------------------------------------------------------------
5.3 UDPSendContext + UDPSendContextPool
---------------------------------------------------------------------

We introduce a fixed pool of send contexts. Each in-flight UDP send owns one context.

Design constraints:
- No heap allocation per send.
- Context must be trivially destructible.
- Context must keep header bytes + payload reference until completion.

Concept type:

```cpp
struct UDPSendContext
{
    std::array<uint8_t, System::UDPTransportHeader::SIZE> headerBytes;
    PacketMessage* payload = nullptr;     // consumed reference
    uint16_t payloadLen = 0;
    boost::asio::ip::udp::endpoint destination;
};
```

Pool guidelines:
- Pre-allocate N contexts at startup (start with 256 or 1024).
- Provide Acquire()/Release() that are O(1).
- Simplest acceptable: `moodycamel::ConcurrentQueue<UDPSendContext*>` of pointers.
- If pool is exhausted: rate-limited log + drop (do not crash, do not allocate).


---------------------------------------------------------------------
5.4 Header Encoding (byte-accurate, no packed stores)
---------------------------------------------------------------------

On-wire header layout (current protocol):
- tag (1 byte)
- sessionId (8 bytes)
- udpToken (16 bytes, uint128_t)

Phase 1 keeps current endianness/layout (no conversions). This preserves compatibility.

Encoding helper (pseudocode):

```cpp
static_assert(System::UDPTransportHeader::SIZE == 25);

static constexpr size_t OFF_TAG = 0;
static constexpr size_t OFF_SESSION_ID = OFF_TAG + sizeof(uint8_t);
static constexpr size_t OFF_TOKEN = OFF_SESSION_ID + sizeof(uint64_t);

inline void EncodeUdpTransportHeader(
    std::array<uint8_t, System::UDPTransportHeader::SIZE>& out,
    uint8_t tag,
    uint64_t sessionId,
    uint128_t udpToken)
{
    std::memcpy(out.data() + OFF_TAG, &tag, sizeof(tag));
    std::memcpy(out.data() + OFF_SESSION_ID, &sessionId, sizeof(sessionId));
    std::memcpy(out.data() + OFF_TOKEN, &udpToken, sizeof(udpToken));
}
```


---------------------------------------------------------------------
5.5 The only send API we use: UDPNetworkImpl::AsyncSend
---------------------------------------------------------------------

We define an internal API that makes the ownership rules explicit.

Recommended signature:

```cpp
// Always safe to call from any thread.
// Ownership contract: consumes exactly 1 reference to `payload`.
void UDPNetworkImpl::AsyncSend(
    const boost::asio::ip::udp::endpoint& destination,
    uint8_t tag,
    uint64_t sessionId,
    uint128_t udpToken,
    PacketMessage* payload,
    uint16_t payloadLen);
```

Ownership contract (CRITICAL):
- AsyncSend consumes exactly 1 reference to payload.
- Completion handler MUST call `MessagePool::Free(payload)` exactly once.

Caller rules:
- If caller transfers ownership (most common): caller must NOT free payload after AsyncSend.
- If caller needs to keep using payload: caller must `payload->AddRef()` before AsyncSend, and later
  free its own reference.

Datagram sizing enforcement:
- If `payloadLen > UDP_MAX_APP_BYTES`: drop + log + metric, and consume the payload reference
  (i.e., call `MessagePool::Free(payload)` immediately).

Completion handler rules (never break these):
- Always free payload and return context to pool even on error/operation_aborted/shutdown.
- Do NOT capture or touch `UDPSession*` in completion.
  Sessions may be recycled/destroyed before send completes.


---------------------------------------------------------------------
5.6 UDPSession raw UDP Flush (no payload memcpy)
---------------------------------------------------------------------

Raw UDP path (SendUnreliable) already enqueues a `PacketMessage*` to `_sendQueue`.
In Flush, we stop building vectors and instead call AsyncSend per dequeued packet.

Important correctness note:
- UDP does not have a TCP-style async_write chain.
- After draining the queue, we MUST set `_isSending=false` to allow future flush triggers.

Concrete control-flow pseudocode:

```cpp
auto drainAndSendAll = [&]() {
    PacketMessage* msg = nullptr;
    while (_sendQueue.try_dequeue(msg))
    {
        _impl->network->AsyncSend(
            _impl->endpoint,
            System::UDPTransportHeader::TAG_RAW_UDP,
            _id,
            _impl->udpToken,
            msg,
            msg->length);
    }
};

drainAndSendAll();

_isSending.store(false, std::memory_order_release);

// Straggler re-check
PacketMessage* straggler = nullptr;
if (_sendQueue.try_dequeue(straggler))
{
    if (_isSending.exchange(true, std::memory_order_acq_rel) == false)
    {
        _sendQueue.enqueue(straggler);
        Flush();
    }
    else
    {
        _sendQueue.enqueue(straggler);
    }
}
```


---------------------------------------------------------------------
5.7 UDPSession KCP output callback (one memcpy, no vector)
---------------------------------------------------------------------

KCP provides a transient buffer to the output callback.
We must copy it once into a pooled `PacketMessage` to keep it alive.

Algorithm:
1) `PacketMessage* msg = MessagePool::AllocatePacket(len);`
2) `memcpy(msg->Payload(), buf, len);`
3) `_impl->network->AsyncSend(endpoint, TAG_KCP, sessionId, token, msg, (uint16_t)len);`

No temporary vector.


---------------------------------------------------------------------
5.8 Handler allocation (optional optimization)
---------------------------------------------------------------------

Even if payloads are pooled, Asio may allocate for handler objects.
Optional follow-up:
- use `boost::asio::bind_allocator` with a fixed handler allocator.

Do NOT add this until Phase 1 correctness is proven.


=====================================================================
6) Commit Plan (Do This In Order)
=====================================================================

This section defines a commit-by-commit sequence. Do NOT split this document into multiple files.

Each commit should be reviewable and runnable.

NOTE (TCP coexistence)
- UDP changes must not reduce TCP parallelism.
- UDP uses one shared socket, so we use a UDP-only strand. This does not serialize TCP sockets.
- Verification must include TCP + UDP traffic concurrently.

Commit 1: Add UDP datagram limit constants (no behavior change)
- Add `src/System/Network/UDPLimits.h` with:
  - `UDP_MAX_DATAGRAM_BYTES` (default 1200)
  - `UDP_MAX_APP_BYTES` (= max datagram - UDPTransportHeader::SIZE)
- Add a brief comment: "MMORPG low-latency default: avoid fragmentation".
- No callers yet (or only compile-time references).

Commit 2: Introduce UDPSendContext and UDPSendContextPool (unused)
- Add new files under `src/System/Network/`:
  - `UDPSendContextPool.h`
  - `UDPSendContextPool.cpp`
- Pool requirements:
  - pre-allocate N contexts
  - Acquire/Release are O(1)
  - no heap allocation on Acquire/Release
  - exhaustion behavior: drop path supported (returns nullptr)

Commit 3: Add UDP strand + AsyncSend API to UDPNetworkImpl (still keep old SendTo)
- Update `src/System/Network/UDPNetworkImpl.h`
  - add `_strand`
  - add `AsyncSend(destination, tag, sessionId, udpToken, payload, payloadLen)`
  - keep existing `SendTo(const uint8_t*, ...)` for now to reduce blast radius
- Update `src/System/Network/UDPNetworkImpl.cpp`
  - construct `_strand` with `make_strand(_socket.get_executor())`
  - implement AsyncSend:
    - enforce `payloadLen <= UDP_MAX_APP_BYTES` (drop on exceed)
    - acquire UDPSendContext; on failure: drop + free payload
    - encode header to ctx->headerBytes (memcpy offsets)
    - `post(_strand, ...)` then `async_send_to` with 2 buffers
    - completion: always `MessagePool::Free(payload)` and return context

Commit 4: Fix UDPSession raw UDP Flush to use AsyncSend + fix `_isSending`
- Update `src/System/Session/UDPSession.cpp`:
  - remove vector allocation + payload memcpy from `UDPSession::Flush()`
  - call `_impl->network->AsyncSend(... TAG_RAW_UDP ...)` per dequeued message
  - implement `_isSending=false` reset + straggler re-check
- Important: Flush must NOT free `PacketMessage*` locally; AsyncSend consumes 1 ref.

Commit 5: Fix KCP output callback to use pooled PacketMessage + AsyncSend
- Update `src/System/Session/UDPSession.cpp`:
  - replace vector allocation in KCP output callback
  - `AllocatePacket(len)` + `memcpy(msg->Payload(), buf, len)`
  - call AsyncSend with TAG_KCP

Commit 6 (optional but recommended): Remove vector in UDPSession::SendReliable
- Update `src/System/Session/UDPSession.cpp`:
  - avoid `std::vector<uint8_t>` when serializing to KCP
  - serialize into pooled PacketMessage and feed KCP from it
  - free pooled message after `kcp->Send` returns

Commit 7 (cleanup): Deprecate old UDPNetworkImpl::SendTo if unused
- If all UDP callers migrated, remove or restrict `SendTo(const uint8_t*, ...)`.
- Ensure no other modules depend on it.


=====================================================================
7) Verification / Acceptance Criteria
=====================================================================

Safety
- No UAF: run extended UDP stress for 60+ seconds with 100+ sessions.
- Shutdown safety: Stop/close during heavy sends should not leak contexts or packets.

Performance
- UDP send path contains no per-send `std::vector<uint8_t>` allocations.
- Raw UDP send path does not memcpy payload into a temporary buffer.

Datagram sizing
- Enforce: `UDPTransportHeader::SIZE + payloadLen <= UDP_MAX_DATAGRAM_BYTES`.
- Oversize behavior: drop + rate-limited log + metric.

Correctness
- Client still parses `UDPTransportHeader` + packet bytes.
- TCP/Dispatcher behavior unchanged (`PacketMessage::Payload()` semantics preserved).

TCP + UDP coexistence
- Verify TCP and UDP can both sustain traffic concurrently:
  - TCP: accept connections and run normal session traffic while UDP spam is active.
  - UDP: high-rate unreliable sends + KCP reliable sends.
- Confirm UDP strand does not regress TCP throughput (it should not; it is UDP-only).


=====================================================================
8) Pitfalls (DO / DON'T)
=====================================================================

DO
- Post all UDP send initiations to UDPNetworkImpl strand.
- Always free payload in completion handler.
- Keep UDPSendContext trivially destructible.
- Enforce datagram sizing.

DON'T
- Don't pass stack or thread_local buffers to `async_send_to`.
- Don't store PacketPtr / shared_ptr / std::function inside pooled messages.
- Don't globally change MessagePool/PacketMessage layout for UDP headroom.
- Don't touch UDPSession from UDP send completion handlers.
