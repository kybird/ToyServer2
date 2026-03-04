#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>


namespace SimpleGame {

class Player;
class Room;

/**
 * @brief 명령어가 실행되는 맥락 정보
 */
struct CommandContext
{
    uint64_t sessionId = 0; // 명령어를 보낸 세션 ID (0이면 서버 콘솔)
    int32_t roomId = 1;     // 대상 룸 ID
    bool isGM = false;      // 관리자 권한 여부

    // 유틸리티 함수
    bool FromServer() const
    {
        return sessionId == 0;
    }
};

using CommandHandler = std::function<void(const CommandContext &ctx, const std::vector<std::string> &args)>;

struct CommandInfo
{
    std::string command;
    std::string description;
    CommandHandler handler;
};

/**
 * @brief 서버 터미널 및 인게임 채팅 명령어를 통합 관리하는 클래스
 */
class CommandManager
{
public:
    static CommandManager &Instance()
    {
        static CommandManager instance;
        return instance;
    }

    void Init();

    /**
     * @brief 명령어 등록
     */
    void RegisterCommand(const std::string &command, const std::string &description, CommandHandler handler);

    /**
     * @brief 문자열 명령 실행
     * @param line 입력받은 전체 문자열 (예: "/spawn 1 10")
     */
    void Execute(const CommandContext &ctx, const std::string &line);

    const std::map<std::string, CommandInfo> &GetCommands() const
    {
        return _commands;
    }

private:
    CommandManager() = default;

    std::map<std::string, CommandInfo> _commands;
};

} // namespace SimpleGame
