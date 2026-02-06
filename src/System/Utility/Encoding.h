#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace System {
namespace Utility {

/**
 * @brief 시스템 에러 메시지(ANSI/CP949)를 UTF-8로 변환합니다.
 * 윈도우에서는 변환을 수행하고, 리눅스 등 타 플랫폼에서는 그대로 반환합니다.
 */
inline std::string ToUtf8(const std::string &str)
{
#ifdef _WIN32
    if (str.empty())
        return "";

    // 1. ANSI -> WideChar
    int nwLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    if (nwLen <= 0)
        return str;

    std::wstring wstr(nwLen, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], nwLen);

    // 2. WideChar -> UTF-8
    int nLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (nLen <= 0)
        return str;

    std::string utf8(nLen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], nLen, NULL, NULL);

    // NULL 종료 문자 제거
    if (!utf8.empty() && utf8.back() == '\0')
    {
        utf8.pop_back();
    }

    return utf8;
#else
    // 리눅스 등은 이미 UTF-8이거나 변환이 필요 없는 구조
    return str;
#endif
}

} // namespace Utility
} // namespace System
