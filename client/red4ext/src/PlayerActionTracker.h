#pragma once

#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <Red/GameObject.hpp>

#include "RED4ext/CName.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/Object.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/events/HitEvent.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/input/ActionType.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/mounting/MountingEvent.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/mounting/UnmountingEvent.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/projectile/ShootEvent.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/weapon/Object.hpp"

class PlayerActionTracker final : public Red::IScriptable
{
public:
    void RecordPlayerAction(RED4ext::CName actionName, RED4ext::gameinputActionType actionType, float value);
    void OnShoot(RED4ext::Handle<RED4ext::gameprojectileShootEvent> event);
    void OnHit(Red::Handle<Red::GameObject> gameObject, RED4ext::Handle<RED4ext::gameHitEvent> event);
    void OnMounting(RED4ext::Handle<RED4ext::game::mounting::MountingEvent> event);
    void OnUnmounting(RED4ext::Handle<RED4ext::game::mounting::UnmountingEvent> event);
    void OnItemEquipped(RED4ext::TweakDBID slot, RED4ext::ItemID item, bool isWeapon);
    void OnItemUnequipped(RED4ext::TweakDBID slot, RED4ext::ItemID item, bool isWeapon);
private:
    RTTI_IMPL_TYPEINFO(PlayerActionTracker);
    RTTI_IMPL_ALLOCATOR();
};

RTTI_DEFINE_CLASS(PlayerActionTracker, {
    RTTI_METHOD(RecordPlayerAction);
    RTTI_METHOD(OnShoot);
    RTTI_METHOD(OnHit);
    RTTI_METHOD(OnMounting);
    RTTI_METHOD(OnUnmounting);
    RTTI_METHOD(OnItemEquipped);
    RTTI_METHOD(OnItemUnequipped);
    RTTI_ALIAS("Cyberverse.Network.Managers.PlayerActionTracker");
});
