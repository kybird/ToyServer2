#pragma once

#include "System/Network/UDPTransportHeader.h"
#include "System/Pch.h"

namespace System {

/**
 * @brief UDP 관련 상수 정의
 * IP 단편화(Fragmentation)를 방지하기 위해 MTU를 고려한 크기를 사용합니다.
 */

// 단일 데이터그램의 최대 크기 (MTU 1500 미만으로 설정하여 단편화 방지)
constexpr uint16_t UDP_MAX_DATAGRAM_BYTES = 1200;

// UDP 전송 헤더 크기 (tag[1] + sessionId[8] + udpToken[16] = 25 bytes)
constexpr uint16_t UDP_TRANSPORT_HEADER_BYTES = UDPTransportHeader::SIZE;

// 애플리케이션 페이로드의 최대 크기
constexpr uint16_t UDP_MAX_APP_BYTES = UDP_MAX_DATAGRAM_BYTES - UDP_TRANSPORT_HEADER_BYTES;

} // namespace System
