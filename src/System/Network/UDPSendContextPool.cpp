#include "System/Network/UDPSendContextPool.h"
#include "System/ILog.h"
#include "System/Pch.h"


namespace System {

UDPSendContextPool &UDPSendContextPool::Instance()
{
    static UDPSendContextPool instance;
    return instance;
}

UDPSendContextPool::~UDPSendContextPool()
{
    // 모든 할당된 컨텍스트를 정리합니다.
    // 큐에 있는 컨텍스트와 현재 사용 중인 컨텍스트 모두 삭제합니다.
    for (UDPSendContext *ctx : _allContexts)
    {
        delete ctx;
    }
    _allContexts.clear();
}

void UDPSendContextPool::Prepare(size_t poolSize)
{
    std::lock_guard<std::mutex> lock(_initMutex);
    if (_initialized)
        return;

    _allContexts.reserve(poolSize);

    for (size_t i = 0; i < poolSize; ++i)
    {
        UDPSendContext *ctx = new UDPSendContext();
        ctx->payload = nullptr;
        ctx->payloadLen = 0;

        _allContexts.push_back(ctx);
        _pool.enqueue(ctx);
    }

    _initialized = true;
    LOG_INFO("UDPSendContextPool initialized with {} contexts.", poolSize);
}

UDPSendContext *UDPSendContextPool::Acquire()
{
    UDPSendContext *ctx = nullptr;
    if (_pool.try_dequeue(ctx))
    {
        return ctx;
    }
    return nullptr;
}

void UDPSendContextPool::Release(UDPSendContext *ctx)
{
    if (!ctx)
        return;

    // [Safety] 이중 반납 및 ABA 방지를 위한 검증
    // 반납 시점에는 이미 payload가 메시지 풀로 돌아갔거나 소유권이 정리되어 있어야 합니다.
    assert(ctx->payload == nullptr);
    assert(ctx->payloadLen == 0);

    // 다시 한번 명시적 초기화 후 반납
    ctx->payload = nullptr;
    ctx->payloadLen = 0;

    _pool.enqueue(ctx);
}

} // namespace System
