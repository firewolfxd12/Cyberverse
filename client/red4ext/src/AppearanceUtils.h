#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const Red::Handle<Red::GameObject>& player);
}
