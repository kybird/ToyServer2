#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace System {

// JSON Library Abstraction using Type Alias
// This allows us to switch the underlying library relatively easily in the future
// while keeping the convenient syntax of nlohmann::json.
using Json = nlohmann::json;

// Helper to standardizing serialization
inline std::string ToJsonString(const Json &json)
{
    return json.dump();
}

inline Json FromJsonString(const std::string &str)
{
    return Json::parse(str);
}

} // namespace System
