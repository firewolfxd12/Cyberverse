#pragma once
#include <RED4ext/RED4ext.hpp>
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const RED4ext::Handle<RED4ext::GameObject>& player);
}
