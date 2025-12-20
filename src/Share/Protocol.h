#pragma once

#include "System/Packet/PacketHeader.h"

namespace Share {

enum PacketType : uint16_t {
    PKT_C_LOGIN = 1000,
    PKT_S_LOGIN = 1001,
    PKT_C_ECHO = 2000,
    PKT_S_ECHO = 2001,
};

using PacketHeader = System::PacketHeader;

} // namespace Share
