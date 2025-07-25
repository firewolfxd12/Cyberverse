#include "PlayerActionTracker.h"
#include "Main.h"
#include "RED4ext/Api/Sdk.hpp"
#include <RedLib.hpp>
#include <Red/GameObject.hpp>

#include "NetworkGameSystem.h"
#include "RED4ext/Scripting/Natives/Generated/game/MountEventData.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/damage/AttackData.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/mounting/MountingRequest.hpp"
#include "Utils.h"

#include <serverbound/WorldPacketsServerBound.h>

void PlayerActionTracker::RecordPlayerAction(RED4ext::CName actionName, RED4ext::gameinputActionType actionType,
                                             float value)
{
    const auto name = std::string(actionName.ToString());
    // SDK->logger->InfoF(PLUGIN, "Player Action %s (%d) -> %f", name.c_str(), actionType, value);

    // We ignore movement inputs like Forward, MoveX, MoveY for now, because we don't care about animations but just
    // want the most recent position And for that, those inputs are very unreliable due to physics/collision etc anyway.
    if (actionType == RED4ext::game::input::ActionType::RELATIVE_CHANGE)
    {
        // think about mouse movements, at least mouse_x is important as yaw.
        // alternatively CameraMouseX.
    }
    else if (name == "Jump" && actionType == RED4ext::game::input::ActionType::BUTTON_RELEASED)
    {
        const auto player = Cyberverse::Utils::GetPlayer();
        const auto [X, Y, Z, W] = Cyberverse::Utils::Entity_GetWorldPosition(player);

        PlayerActionTracked tracked = {};
        // BUTTON_RELEASED means the key went up, all other types mark it as pressed
        tracked.buttonState = actionType != RED4ext::game::input::ActionType::BUTTON_RELEASED;
        tracked.networkTick = Red::GetGameSystem<NetworkGameSystem>()->NextNetworkTick();
        tracked.action = eACTION_JUMP;

        tracked.worldTransform = {};
        tracked.worldTransform.x = X;
        tracked.worldTransform.y = Y;
        tracked.worldTransform.z = Z;

        Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(1, tracked);
    }
}

void PlayerActionTracker::OnShoot(RED4ext::Handle<RED4ext::gameprojectileShootEvent> event)
{
    PlayerShoot player_shoot = {};
    player_shoot.charge = event->params.charge;
    player_shoot.startPoint = Vector3 { event->startPoint.X, event->startPoint.Y, event->startPoint.Z };
    if (!event->weapon.Expired())
    {
        auto weapon = event->weapon.Lock();
        player_shoot.itemIdWeapon = Cyberverse::Utils::GameObject_GetRecordID(weapon).value;
    }
    else
    {
        player_shoot.itemIdWeapon = 0;
    }
    Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, player_shoot);
}

void PlayerActionTracker::OnHit(Red::Handle<Red::GameObject> gameObject,
                                RED4ext::Handle<RED4ext::gameHitEvent> event)
{
    SDK->logger->InfoF(PLUGIN, "OnHit! %d", event->attackData->attackType);
}

void PlayerActionTracker::OnMounting(RED4ext::Handle<RED4ext::game::mounting::MountingEvent> event)
{
    if (event->relationship.otherMountableType != RED4ext::game::MountingObjectType::Vehicle)
    {
        // mountingSubType would tell us whether bike or car.
        return;
    }

    if (event->relationship.relationshipType != RED4ext::game::MountingRelationshipType::Parent)
    {
        // If the car mounts us, do nothing
        return;
    }

    if (event->relationship.otherObject.Expired())
    {
        SDK->logger->Warn(PLUGIN,
                          "Cannot Process PlayerActionTracker::OnMounting as the vehicle's weak ref has expried");
        return;
    }

    // Keep alive as long as the derived vehicleInstance
    const auto strongLock = event->relationship.otherObject.Lock();
    // TODO: if you figure out how to inline that, you're welcome to do so.
    const RED4ext::game::Object* ptr = strongLock;
    const auto vehicleInstance = RED4ext::Handle((RED4ext::VehicleObject*)ptr);

    // TODO: vehicles.swift, there is VehicleObject.GetRecordID(), but there is also GameObject::GetTDBID(go) which is
    // more generic and handles casting, so it works for puppets, devices _and_ vehicles. Question is if we should here
    // just implement both so we can avoid casting and misleading?

    const auto recordId = Cyberverse::Utils::VehicleObject_GetRecordID(vehicleInstance);
    const auto [X, Y, Z, W] = Cyberverse::Utils::Entity_GetWorldPosition(vehicleInstance);
    const auto orientation = Cyberverse::Utils::Entity_GetWorldOrientation(vehicleInstance);
    const auto [Roll, Pitch, Yaw] = Cyberverse::Utils::Quaternion_ToEulerAngles(orientation);

    PlayerSpawnCar spawnCar = {};
    spawnCar.recordId = recordId.value;
    spawnCar.worldTransform = Vector3{X, Y, Z};
    spawnCar.yaw = Yaw;

    Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, spawnCar);
}

void PlayerActionTracker::OnUnmounting(RED4ext::Handle<RED4ext::game::mounting::UnmountingEvent> event)
{
    if (event->relationship.otherMountableType != RED4ext::game::MountingObjectType::Vehicle)
    {
        // mountingSubType would tell us whether bike or car.
        return;
    }

    if (event->relationship.relationshipType != RED4ext::game::MountingRelationshipType::Parent)
    {
        // If the car mounts us, do nothing
        return;
    }

    if (event->relationship.otherObject.Expired())
    {
        SDK->logger->Warn(PLUGIN, "Cannot Process PlayerActionTracker::OnUnmounting as the vehicle's weak ref "
                                  "has expried");
        return;
    }

    // Keep alive as long as the derived vehicleInstance
    const auto strongLock = event->relationship.otherObject.Lock();
    // TODO: if you figure out how to inline that, you're welcome to do so.
    const RED4ext::game::Object* ptr = strongLock;
    const auto vehicleInstance = RED4ext::Handle((RED4ext::VehicleObject*)ptr);

    auto entityId = vehicleInstance->entityID;
    const auto netIdOpt = Red::GetGameSystem<NetworkGameSystem>()->GetNetworkedEntityId(entityId);

    PlayerUnmountCar unmount_car = {};
    if (netIdOpt.has_value())
    {
        unmount_car.networkedEntityId = netIdOpt.value();
    }
    else
    {
        unmount_car.networkedEntityId = 0;
    }

    Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, unmount_car);
}

void PlayerActionTracker::OnItemEquipped(const RED4ext::TweakDBID slot, const RED4ext::ItemID item, const bool isWeapon)
{
    SDK->logger->InfoF(PLUGIN, "Item Equipped at slot %llu with item %llu", slot.value, item.tdbid.value);
    PlayerEquipItem player_equip = {};
    player_equip.slot = slot.value;
    player_equip.itemId = item.tdbid.value;
    player_equip.isWeapon = isWeapon;
    player_equip.isUnequipping = false;
    Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, player_equip);
}

void PlayerActionTracker::OnItemUnequipped(const RED4ext::TweakDBID slot, const RED4ext::ItemID item, const bool isWeapon)
{
    SDK->logger->InfoF(PLUGIN, "Item Unequipped at slot %llu with item %llu", slot.value, item.tdbid.value);
    PlayerEquipItem player_equip = {};
    player_equip.slot = slot.value;
    player_equip.itemId = item.tdbid.value;
    player_equip.isWeapon = isWeapon;
    player_equip.isUnequipping = true;
    Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, player_equip);
}
