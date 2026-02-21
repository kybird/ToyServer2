#include "System/Dispatcher/MessagePool.h"
#include "System/Network/UDPLimits.h"
#include "System/Network/UDPNetworkImpl.h"
#include "System/Pch.h"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief UDP AsyncSend 패스 검증을 위한 툴
 * UDPNetworkImpl::AsyncSend를 직접 호출하여 정상 크기 및 오버사이즈 드랍을 검증합니다.
 *
 * 사용법:
 *   UdpSpamClient.exe --mode asyncsend --payload 200 --count 50000 --dest 127.0.0.1:9999
 *   UdpSpamClient.exe --mode asyncsend --payload 1300 --count 10000 --dest 127.0.0.1:9999
 */
int main(int argc, char *argv[])
{
    // 기본값
    std::string mode = "asyncsend";
    uint16_t payloadSize = 200; // 기본: 정상 크기
    uint32_t sendCount = 50000; // 기본: 50000개 전송
    std::string destStr = "127.0.0.1:9999";

    // 커맨드라인 인자 파싱
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
        {
            mode = argv[++i];
        }
        else if (arg == "--payload" && i + 1 < argc)
        {
            payloadSize = static_cast<uint16_t>(std::stoul(argv[++i]));
        }
        else if (arg == "--count" && i + 1 < argc)
        {
            sendCount = static_cast<uint32_t>(std::stoul(argv[++i]));
        }
        else if (arg == "--dest" && i + 1 < argc)
        {
            destStr = argv[++i];
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: UdpSpamClient.exe [options]\n"
                      << "Options:\n"
                      << "  --mode <mode>      Execution mode (asyncsend, default: asyncsend)\n"
                      << "  --payload <bytes>   Payload size in bytes (default: 200)\n"
                      << "  --count <number>    Number of sends (default: 50000)\n"
                      << "  --dest <address>    Destination endpoint (default: 127.0.0.1:9999)\n"
                      << "  --help              Show this help\n"
                      << "\nExamples:\n"
                      << "  UdpSpamClient.exe --mode asyncsend --payload 200 --count 50000 --dest 127.0.0.1:9999\n"
                      << "  UdpSpamClient.exe --mode asyncsend --payload 1300 --count 10000 --dest 127.0.0.1:9999\n";
            return 0;
        }
    }

    // 목적지 파싱
    size_t colonPos = destStr.find(':');
    if (colonPos == std::string::npos)
    {
        std::cerr << "Error: Invalid destination format. Use IP:PORT (e.g., 127.0.0.1:9999)" << std::endl;
        return 1;
    }
    std::string ipStr = destStr.substr(0, colonPos);
    uint16_t port = static_cast<uint16_t>(std::stoul(destStr.substr(colonPos + 1)));

    std::cout << "UDP Send Path Verification Tool\n"
              << "Mode: " << mode << "\n"
              << "Payload: " << payloadSize << " bytes (max: " << System::UDP_MAX_APP_BYTES << ")\n"
              << "Count: " << sendCount << "\n"
              << "Destination: " << destStr << "\n"
              << std::endl;

    if (mode == "asyncsend")
    {
        try
        {
            // MessagePool 초기화
            System::MessagePool::Prepare(8192);

            boost::asio::io_context ioContext;
            System::UDPNetworkImpl udpNet(ioContext);

            // UDP 소켓 바인드 (ephemeral port: 0)
            if (!udpNet.Start(0))
            {
                std::cerr << "Error: Failed to start UDP network" << std::endl;
                return 1;
            }

            boost::asio::ip::udp::endpoint dest(boost::asio::ip::make_address(ipStr), port);

            std::atomic<uint32_t> sentCount{0};
            std::atomic<uint32_t> failedCount{0};
            bool isOversize = payloadSize > System::UDP_MAX_APP_BYTES;

            // 송신 스레드
            std::thread sendThread(
                [&]()
                {
                    for (uint32_t i = 0; i < sendCount; ++i)
                    {
                        // 패킷 할당 및 페이로드 채우기
                        auto *packet = System::MessagePool::AllocatePacket(payloadSize);
                        if (!packet)
                        {
                            std::cerr << "Error: Failed to allocate packet" << std::endl;
                            failedCount++;
                            continue;
                        }

                        // 페이로드 채우기 (테스트 데이터)
                        uint8_t *payload = packet->Payload();
                        for (uint16_t j = 0; j < payloadSize; ++j)
                        {
                            payload[j] = static_cast<uint8_t>(j & 0xFF);
                        }

                        // UDPTransportHeader 값
                        uint8_t tag = System::UDPTransportHeader::TAG_RAW_UDP;
                        uint64_t sessionId = 0x123456789ABCDEF0ULL;
                        System::uint128_t udpToken(0x5566778899AABBCCULL, 0xAABBCCDD11223344ULL);

                        // AsyncSend 호출 (소유권 전달)
                        udpNet.AsyncSend(dest, tag, sessionId, udpToken, packet, payloadSize);
                        sentCount++;

                        // 속도 제어 (선택적 - 과부하 방지)
                        if (!isOversize && (i % 1000 == 0))
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }
                }
            );

            // 통계 출력 스레드
            std::thread statThread(
                [&]()
                {
                    uint32_t lastCount = 0;
                    for (int i = 0; i < 30; ++i) // 최대 30초 대기
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        uint32_t current = sentCount.load();
                        std::cout << "Progress: " << current << " / " << sendCount << " (+" << (current - lastCount)
                                  << " pps)" << std::endl;
                        lastCount = current;

                        // 모든 전송 완료 확인
                        if (current >= sendCount)
                        {
                            break;
                        }
                    }
                }
            );

            // io_context 실행 (비동기 완료 처리)
            std::thread ioThread(
                [&]()
                {
                    ioContext.run();
                }
            );

            // 송신 스레드 대기
            sendThread.join();
            std::cout << "\nSend thread finished. Waiting for completion..." << std::endl;

            // 완료 대기 (최대 10초)
            statThread.join();

            // io_context 정지
            ioContext.stop();
            ioThread.join();

            // 결과 출력
            uint64_t oversizeDrops = udpNet.GetOversizeDrops();
            std::cout << "\n=== Results ===\n"
                      << "Total requested: " << sendCount << "\n"
                      << "Sent: " << sentCount.load() << "\n"
                      << "Failed: " << failedCount.load() << "\n"
                      << "Oversize drops: " << oversizeDrops << "\n"
                      << std::endl;

            // 검증
            if (isOversize)
            {
                if (oversizeDrops > 0)
                {
                    std::cout << "PASS: Oversize packets were correctly dropped" << std::endl;
                    return 0;
                }
                else
                {
                    std::cerr << "FAIL: Expected oversize drops, but none recorded" << std::endl;
                    return 1;
                }
            }
            else
            {
                if (oversizeDrops == 0)
                {
                    std::cout << "PASS: Normal packets sent without oversize drops" << std::endl;
                    return 0;
                }
                else
                {
                    std::cerr << "FAIL: Unexpected oversize drops for normal-sized packets" << std::endl;
                    return 1;
                }
            }
        } catch (const std::exception &e)
        {
            std::cerr << "Exception: " << e.what() << std::endl;
            return 1;
        }
    }
    else
    {
        std::cerr << "Error: Unknown mode '" << mode << "'. Use --help for usage." << std::endl;
        return 1;
    }

    return 0;
}
