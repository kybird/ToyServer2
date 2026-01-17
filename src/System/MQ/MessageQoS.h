#pragma once

namespace System::MQ {

enum class MessageQoS {
    Fast,    // NATS: Fire-and-Forget, Low Latency
    Reliable // Redis Stream: Persistent, Ordered
};

}
