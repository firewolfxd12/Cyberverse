#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/Object.hpp>
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const Red::Handle<Red::GameObject>& player);
}
