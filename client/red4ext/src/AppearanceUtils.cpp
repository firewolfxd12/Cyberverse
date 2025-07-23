#include "AppearanceUtils.h"
#include "RED4ext/Scripting/Functions.hpp"
#include "RedLib.hpp"
#include <Red/GameObject.hpp>
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const Red::Handle<Red::GameObject>& player)
{
    // Placeholder implementation: only capture appearance name
    RED4ext::CName appName;
    Red::CallVirtual(player, "GetCurrentAppearanceName", appName);
    return std::string(appName.ToString());
}
}
