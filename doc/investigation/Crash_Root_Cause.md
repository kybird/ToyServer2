# 🛑 Crash Root Cause Analysis

## Issue
**Integer Truncation & Heap Buffer Overflow**

## Discovery
- **Test**: `CrashReproductionTests.cpp` [Test 5: Large_Packet_Truncation]
- **Result**: CRASH (Segfault / Heap Corruption)

## Technical Details
1. **The Trap**: 
   - `PacketBase.h` defines `MaxPacketSize` as `1MB` (1048576).
   - But `GetTotalSize()` returns `uint16_t` (Max 65535).
   - `PacketHeader.h` also uses `uint16_t size`.

2. **The Trigger**:
   - `Room_Update.cpp` collects ALL objects (Projectiles + Monsters + Items) into a **single** `S_MoveObjectBatch` packet.
   - User reported "Many projectiles... many monsters" (approx > 2000 objects).
   - Payload estimation: 2500 objects * ~35 bytes = **87,500 bytes**.

3. **The Failure Chain**:
   - `ByteSizeLong()` returns `87,500`.
   - `GetTotalSize()` checks `87,500 <= 1MB` (Assertion PASS).
   - `GetTotalSize()` casts `87,500` to `uint16_t`.
   - `87,500 (0x155CC)` -> truncated to `21,964 (0x55CC)`.
   - `BroadcastPacket` allocates buffer of size `21,964 + safety`.
   - `SerializeTo` writes the full `87,500` bytes.
   - **Buffer Overflow** of ~65KB into the heap.
   - **CRASH**.

## Proposed Solution
1. **Fix `Room_Update.cpp`**: Implement **Chunking**.
   - Split `S_MoveObjectBatch` into multiple packets if `moves.size()` exceeds a safe limit (e.g., 500 or 1000).
2. **Safety Fix**: Update `PacketBase.h`
   - Change assertion to `assert(totalSize <= 65535)`.
