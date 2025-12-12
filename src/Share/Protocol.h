#pragma once

#include <cstdint>

namespace Share {

enum PacketType : uint16_t {
  PKT_C_LOGIN = 1000,
  PKT_S_LOGIN = 1001,
  PKT_C_ECHO = 2000,
  PKT_S_ECHO = 2001,
};

#pragma pack(push, 1)
struct PacketHeader {
  uint16_t size; // Total size including header
  uint16_t id;   // PacketType
};
#pragma pack(pop)

} // namespace Share
