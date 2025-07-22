#include "AppearanceUtils.h"
#include "RED4ext/Scripting/Functions.hpp"
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const RED4ext::Handle<RED4ext::GameObject>& player)
{
    // Placeholder implementation: only capture appearance name
    RED4ext::CName appName;
    RED4ext::CallVirtual(player, "GetCurrentAppearanceName", appName);
    return std::string(appName.ToString());
}
}
