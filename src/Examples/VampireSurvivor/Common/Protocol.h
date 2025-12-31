#pragma once

#include "Protocol/game.pb.h"
#include "System/Packet/PacketHeader.h"

namespace SimpleGame {

using PacketID = Protocol::MsgId;

using PacketHeader = System::PacketHeader;

} // namespace SimpleGame
