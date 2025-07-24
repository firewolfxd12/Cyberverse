#include "AppearanceUtils.h"
#include "RED4ext/Scripting/Functions.hpp"
#include <RedLib.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/Object.hpp>
#include "RED4ext/CName.hpp"
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const Red::Handle<Red::GameObject>& player)
{
    // Placeholder implementation: only capture appearance name
    RED4ext::CName appName;
    Red::CallVirtual(player, "GetCurrentAppearanceName", appName);
    return std::string(appName.ToString());
}

void ApplyAppearance(const Red::Handle<Red::GameObject>& entity, const std::string& appearanceName)
{
    RED4ext::CName appCName(appearanceName.c_str());
    Red::CallVirtual(entity, "ScheduleAppearanceChange", appCName);
}
}
