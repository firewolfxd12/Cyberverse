#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <Red/GameObject.hpp>
#include <string>

namespace Cyberverse::AppearanceUtils {
std::string GetPlayerAppearanceJson(const Red::Handle<Red::GameObject>& player);
}
