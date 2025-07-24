#include "NetworkGameSystem.h"
#include <RedLib.hpp>

#include "CommandLine.h"
#include "Main.h"
#include "Utils.h"

#include "RED4ext/RTTISystem.hpp"
#include "RED4ext/Scripting/Natives/Generated/EulerAngles.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/TeleportationFacility.hpp"
#include "RED4ext/Scripting/Utils.hpp"
#include "RED4ext/SystemUpdate.hpp"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h> // Required, see https://github.com/ValveSoftware/GameNetworkingSockets/issues/171^
#include <steam/steamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>

#include "serverbound/AuthPacketsServerBound.h"
#include "serverbound/WorldPacketsServerBound.h"
#include "serverbound/AppearancePacketsServerBound.h"
#include "clientbound/AuthPacketsClientBound.h"
#include "clientbound/WorldPacketsClientBound.h"
#include "clientbound/AppearancePacketsClientBound.h"
#include <cstring>
#include "AppearanceUtils.h"

#include <optional>
#include <vector>

#include <zpp_bits.h>

bool NetworkGameSystem::Load()
{
    if (SteamDatagramErrMsg errMsg; !GameNetworkingSockets_Init(nullptr, errMsg))
    {
        SDK->logger->ErrorF(PLUGIN, "Could not initialize GameNetworkingSockets: %s", errMsg);
        return false;
    }

    return true;
}
void NetworkGameSystem::Unload()
{
    GameNetworkingSockets_Kill();
}

bool NetworkGameSystem::ConnectToServer(const std::string& host, uint16_t port)
{
    SDK->logger->InfoF(PLUGIN, "Trying to connect to server at %s:%d", host.c_str(), port);

    if (m_pInterface != nullptr)
    {
        SDK->logger->Warn(PLUGIN, "Trying to connect while already being connected. Aborting");
        return false;
    }

    m_pInterface = SteamNetworkingSockets();

    if (m_pInterface == nullptr)
    {
        SDK->logger->Error(PLUGIN, "Failed to initialize the networking library");
    }

    // Since I don't want to parse the ip manually and support both IP versions, I need to create a string first...
    const auto connection_string_size = host.length() + 2 + 5; // 2 (':' + \0) and 5 (65535)
    const auto connection_string = new char[connection_string_size];
    memset(connection_string, '\0', connection_string_size);
    sprintf_s(connection_string, connection_string_size, "%s:%d", host.c_str(), port);

    SteamNetworkingIPAddr address = {};
    if (!address.ParseString(connection_string))
    {
        SDK->logger->WarnF(PLUGIN, "Failed to parse connection string \"%s\"", connection_string);
        m_pInterface = nullptr; // prevent polling
        return false;
    }

    SteamNetworkingConfigValue_t opt = {};
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               reinterpret_cast<void*>(ConnectionStatusChangedCallback));

    // m_hConnection = m_pInterface->ConnectToHostedDedicatedServer(identity, 0, 1, &opt);
    m_hConnection = m_pInterface->ConnectByIPAddress(address, 1, &opt);

    if (m_hConnection == k_HSteamNetConnection_Invalid)
    {
        SDK->logger->WarnF(PLUGIN, "Could not create connection for string \"%s\": invalid", connection_string);
        return false;
    }

    return true;
}

void NetworkGameSystem::OnNetworkUpdate(RED4ext::FrameInfo& frame_info, RED4ext::JobQueue& job_queue)
{
    // TODO: make this framerate indepedent, maybe also use multiple UpdateTickGroups.
    if (!m_hasTriedToConnect)
    {
        // We auto-connect on the first tick with the CLI address. We don't connect earlier because the message loop
        // isn't run there yet and we are prone to time out.
        const auto commandLine = GetCommandLineA();
        const auto host = ParseHostFromCommandLine(commandLine);
        const auto port = ParsePortFromCommandLine(commandLine);
        if (host.has_value() && port.has_value())
        {
            ConnectToServer(host.value(), port.value());
        }
        m_hasTriedToConnect = true; // We lie here, to prevent parsing the cli every time.
    }

    if (m_pInterface == nullptr)
    {
        return;
    }

    PollIncomingMessages();
    ProcessPendingEntityUpdates();
    TrackPlayerPosition(frame_info.deltaTime);
    TrackLocomotion(frame_info.deltaTime);
    InterpolatePuppets(frame_info.deltaTime);

    m_pInterface->RunCallbacks(); // This shall be called in a loop.
}

void NetworkGameSystem::OnRegisterUpdates(RED4ext::UpdateRegistrar* aRegistrar)
{
    // TODO: If we have no connection information passed on the command line, we have no reason to even register.
    IGameSystem::OnRegisterUpdates(aRegistrar);
    aRegistrar->RegisterUpdate(RED4ext::UpdateTickGroup::FrameBegin, this, "NetworkUpdate",
        [this](RED4ext::FrameInfo &frame_info, RED4ext::JobQueue &job_queue) {
            this->OnNetworkUpdate(frame_info, job_queue);
    });
}

void NetworkGameSystem::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo)
{
    SDK->logger->InfoF(PLUGIN, "Connection Status Changed (%d): %s", pInfo->m_info.m_eState,
                       pInfo->m_info.m_szEndDebug);

    if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected)
    {
        char buf[255];
        DWORD buf_len = 255;
        GetUserNameA(buf, &buf_len);

        SDK->logger->Info(PLUGIN, "Socket connected, authenticating");
        InitAuthServerBound auth_packet = {};
        auth_packet.protocol_version = PROTOCOL_VERSION_CURRENT;
        auth_packet.username = buf;

        // TODO: Maybe we could manage this singleton access better? But then, the Game's GameSystem Container is the owner of "this"
        Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, auth_packet);
    } else {
        Red::GetGameSystem<NetworkGameSystem>()->FullyConnected = false;
    }
}

template<typename T>
bool NetworkGameSystem::EnqueueMessage(uint8_t channel_id, T content)
{
    auto frame = MessageFrame{};
    frame.channel_id = channel_id;
    content.FillMessageFrame(frame);

    auto [data, out] = zpp::bits::data_out();

    auto zpp_result = out(frame);
    if (zpp::bits::failure(zpp_result))
    {
        // Failed to serialize the frame(!)
        return false;
    }

    zpp_result = out(content);
    if (zpp::bits::failure(zpp_result))
    {
        // Failed to serialize the content
        return false;
    }

    assert(data.size() < std::numeric_limits<uint32_t>::max());

    int flags = k_nSteamNetworkingSend_Reliable;
    if (channel_id == 1)
    {
        flags = k_nSteamNetworkingSend_Unreliable;
    }

    const auto result = m_pInterface->SendMessageToConnection(
        m_hConnection, data.data(), static_cast<uint32_t>(data.size()), flags, nullptr);

    if (result == k_EResultOK)
    {
        return true;
    }

    SDK->logger->ErrorF(PLUGIN, "NetworkGameSystem::EnqueueMessage(%d) => Error %d\n", channel_id, result);
    return false;
}

void NetworkGameSystem::SetEntityPosition(const RED4ext::ent::EntityID entityId, RED4ext::Vector4 worldPosition, float yaw)
{
    const auto entity = Cyberverse::Utils::GetDynamicEntity(entityId);
    // TODO: For most, this is actually a NPCPuppet, for those that aren't, this will crash.

    if (entity.has_value())
    {
        // TODO: Find out if this is an NPCPuppet...
        // AI
        RED4ext::Handle<RED4ext::AICommand> commandRef;
        Red::CallVirtual(this, "TeleportPuppet", commandRef, entity.value(), worldPosition, yaw);
        m_LastTeleportCommand[entityId] = commandRef;

        // random game objects
        RED4ext::EulerAngles angles = { 0.0f, 0.0f, yaw };
        const auto teleportFacility = Red::GetGameSystem<RED4ext::TeleportationFacility>();
        if (!Red::CallVirtual(teleportFacility, "Teleport", entity.value() /*"game object"*/, worldPosition, angles))
        {
            SDK->logger->Warn(PLUGIN, "Failed to teleport");
        }

        // Try: scriptInterface.PushAnimationEvent(n"Jump");
        // if (!Red::CallVirtual(entity.value(), "PushAnimationEvent", "Jump"))
        // {
        //     SDK->logger->Info(PLUGIN, "Could not push jump event.");
        // }
    } else
    {
        SDK->logger->Warn(PLUGIN, "Cannot SetEntityPosition, because the entity hasn't been found");
    }
}


void NetworkGameSystem::PollIncomingMessages()
{
    // TODO: More resonable quit condition.
    while (true)
    {
        ISteamNetworkingMessage* pIncomingMsg = nullptr;
        const auto numMsgs = m_pInterface->ReceiveMessagesOnConnection(m_hConnection, &pIncomingMsg, 1);
        if (numMsgs == 0)
        {
            break;
        }

        if (numMsgs < 0)
        {
            SDK->logger->ErrorF(PLUGIN, "Error polling messages: %d", numMsgs);
            return;
        }

        // TODO: Handle or enqueue the message
        //SDK->logger->InfoF(PLUGIN, "Received a packet with %d bytes", pIncomingMsg->m_cbSize);

        auto [data, in] = zpp::bits::data_in();
        const auto begin = (std::byte*)pIncomingMsg->GetData();
        data.assign(begin, begin + pIncomingMsg->GetSize());

        MessageFrame frame = {};
        if (zpp::bits::failure(in(frame)))
        {
            SDK->logger->Error(PLUGIN, "Faulty packet");
            pIncomingMsg->Release();
            continue;
        }

        // TODO: This should both be more generic probably _AND_ we need to consider how we want to hand this off to C#,
        // given that they may want to control _all_ packet logic. This however depends on how flexible and moddable we
        // want our, e.g. auth handling, to be.
        switch (frame.message_type)
        {
        case EINIT_AUTH_RESULT:
        {
            AuthResultClientBound auth_result_packet = {};
            if (zpp::bits::failure(in(auth_result_packet)))
            {
                SDK->logger->Error(PLUGIN, "Faulty packet: AuthResultClientBound");
                pIncomingMsg->Release();
                continue;
            }

            switch (auth_result_packet.auth_result)
            {
            case EAuthResult_Ok:
                SDK->logger->Info(PLUGIN, "Login accepted");
                FullyConnected = true;
                if (m_hasEnqueuedLoadLastCheckpoint && m_systemRequestsHandler)
                {
                    SDK->logger->Info(PLUGIN, "Loading the savegame");
                    Red::CallVirtual(m_systemRequestsHandler, "LoadLastCheckpoint", false);
                }
                // TODO: Follow-Up action
                break;
            case EAuthResult_ValidationFailed:
                SDK->logger->Warn(PLUGIN, "Login: Validation failed");
                break;
            case EAuthResult_VersionMismatch:
                SDK->logger->Warn(PLUGIN, "Login: Version mismatch");
                break;
            default:
                SDK->logger->ErrorF(PLUGIN, "Unknown auth result: %d", auth_result_packet.auth_result);
            }
        }
        break;

        case eSpawnEntity:
        {
            SpawnEntity spawn_entity = {};
            if (zpp::bits::failure(in(spawn_entity)))
            {
                SDK->logger->Error(PLUGIN, "Faulty packet: SpawnEntity");
                pIncomingMsg->Release();
                continue;
            }

            if (m_networkedEntitiesLookup.contains(spawn_entity.networkedEntityId))
            {
                SDK->logger->WarnF(PLUGIN, "Already have a spawned entity for %llu", spawn_entity.networkedEntityId);
                continue;
            }

            // TODO: separate spawning component
            // TODO: SpawnTransientEntity should return the EntityId for a Map<NetworkedEntityId, LocalEntityId>, especially for further updates.
            SDK->logger->InfoF(PLUGIN, "Spawning entity %llu", spawn_entity.recordId);
            RED4ext::TweakDBID entityName = { spawn_entity.recordId };
            RED4ext::Vector4 worldPosition = { spawn_entity.spawnPosition.x, spawn_entity.spawnPosition.y, spawn_entity.spawnPosition.z, 1.0 };
            RED4ext::Quaternion worldOrientation = { 0.0, 0.0, 0.0, 1.0 };

            RED4ext::ent::EntityID entityId;
            if (!Red::CallVirtual(this, "SpawnTransientEntity", entityId, entityName, worldPosition, worldOrientation))
            {
                SDK->logger->Warn(PLUGIN, "Failed to spawn entity!");
            }

            // TODO: Validation. already contained? Error!
            SDK->logger->InfoF(PLUGIN, "Got Entity Id %llu for networkId %llu", entityId.hash, spawn_entity.networkedEntityId);
            m_networkedEntitiesLookup.insert(std::make_pair(spawn_entity.networkedEntityId, entityId));
        }
        break;

        case eTeleportEntity:
        {
            TeleportEntity teleport = {};
            if (zpp::bits::failure(in(teleport)))
            {
                SDK->logger->Error(PLUGIN, "Faulty packet: TeleportEntity");
                pIncomingMsg->Release();
                continue;
            }

            if (!m_networkedEntitiesLookup.contains(teleport.networkedEntityId))
            {
                SDK->logger->WarnF(PLUGIN, "Entity Teleport packet for unknown networkedEntityId %llu. Map size %d",
                                   teleport.networkedEntityId, m_networkedEntitiesLookup.size());
                break;
            }

            const auto entityId = m_networkedEntitiesLookup[teleport.networkedEntityId];
            const RED4ext::Vector4 worldPosition = { teleport.targetPosition.x, teleport.targetPosition.y, teleport.targetPosition.z, 1.0 };

            // TODO: If teleport flag is set
            //SetEntityPosition(entityId, worldPosition, teleport.yaw);

            auto entity = Cyberverse::Utils::GetDynamicEntity(entityId);
            if (!entity.has_value())
            {
                PendingEntityUpdate upd{};
                upd.id = entityId;
                upd.position = worldPosition;
                upd.yaw = teleport.yaw;
                m_pendingUpdates.push_back(upd);
                SDK->logger->Info(PLUGIN, "Queued TeleportEntity until entity exists");
                break;
            }

            const auto yawSource = Cyberverse::Utils::Quaternion_ToEulerAngles(Cyberverse::Utils::Entity_GetWorldOrientation(entity.value())).Yaw;
            const auto positionSource = Cyberverse::Utils::Entity_GetWorldPosition(entity.value());

            if (positionSource.X == 0.0 && positionSource.Y == 0.0 && positionSource.Z == 0.0)
            {
                // TODO: let's pray that this doesn't have a side effect when players DO reside at (0, 0, 0)
                // The first interpolation would go from (0, 0, 0) to target position, but that would despawn and destroy
                // the entity, since it's too far away.
                SDK->logger->Warn(PLUGIN, "Server Bug: Teleport Entity without the teleport flag for a fresh spawned entity");
                SetEntityPosition(entityId, worldPosition, teleport.yaw);
                break;
            }

            SDK->logger->TraceF(PLUGIN, "New Interpolation Data (entity %llu): yaw %f -> %f, position: (%f, %f, %f) -> (%f, %f, %f)", teleport.networkedEntityId, yawSource, teleport.yaw, positionSource.X, positionSource.Y, positionSource.Z, teleport.targetPosition.x, teleport.targetPosition.y, teleport.targetPosition.z);

            if (m_LastTeleportCommand.contains(entityId))
            {
                // TODO: Alternatively we could query the command and if it's still running just skipping enqueing a new comand
                //  but that probably means more lags/teleports again, because the gap between teleports becomes larger.
                //  but then, does stopping really change a thing? The best would be to _update_ the existing command,
                //  but is that working? who knows!
                const auto commandRef = m_LastTeleportCommand[entityId];
                Red::CallVirtual(this, "StopAICommand", entity.value(), commandRef);
            }

            // TODO: This should probably be a map<id, list<data>>, so that a server that eagerly sends too much data doesn't increase interpolation delay
            //  in contrast, we can just increase the time velocity of the interpolator
            const auto interpolation_data = InterpolationData(Cyberverse::Utils::Vector4To3(positionSource), teleport.targetPosition, yawSource, teleport.yaw, 0.1f);
            m_interpolationData[entityId] = interpolation_data;
        }
        break;

        case eApplyAppearance:
        {
            ApplyAppearance apply = {};
            if (zpp::bits::failure(in(apply)))
            {
                SDK->logger->Error(PLUGIN, "Faulty packet: ApplyAppearance");
                pIncomingMsg->Release();
                continue;
            }

            if (!m_networkedEntitiesLookup.contains(apply.networkedEntityId))
            {
                SDK->logger->WarnF(PLUGIN, "Unknown entity for ApplyAppearance %llu", apply.networkedEntityId);
                break;
            }

            const auto entityId = m_networkedEntitiesLookup[apply.networkedEntityId];
            auto entity = Cyberverse::Utils::GetDynamicEntity(entityId);
            if (!entity.has_value())
            {
                PendingEntityUpdate upd{};
                upd.id = entityId;
                upd.appearance = std::string(reinterpret_cast<char*>(apply.data.data()), apply.dataLength);
                m_pendingUpdates.push_back(upd);
                SDK->logger->Info(PLUGIN, "Queued ApplyAppearance until entity exists");
                break;
            }

            std::string appearance(reinterpret_cast<char*>(apply.data.data()), apply.dataLength);
            const Red::Entity* basePtr = entity.value();
            auto obj = Red::Handle<Red::GameObject>(reinterpret_cast<Red::GameObject*>(const_cast<Red::Entity*>(basePtr)));
            if (obj)
            {
                Cyberverse::AppearanceUtils::ApplyAppearance(obj, appearance);
            }
        }
        break;

        case eDestroyEntity:
        {
            DestroyEntity destroy_entity = {};
            if (zpp::bits::failure(in(destroy_entity)))
            {
                SDK->logger->Error(PLUGIN, "Faulty packet: DestroyEntity");
                pIncomingMsg->Release();
                continue;
            }

            if (!m_networkedEntitiesLookup.contains(destroy_entity.networkedEntityId))
            {
                SDK->logger->WarnF(PLUGIN, "Have no spawned entity for %llu", destroy_entity.networkedEntityId);
                continue;
            }

            const auto entityId = m_networkedEntitiesLookup[destroy_entity.networkedEntityId];
            if (!Red::CallVirtual(this, "DestroyTransientEntity", entityId))
            {
                SDK->logger->Warn(PLUGIN, "Failed to destroy entity!");
            }

            m_networkedEntitiesLookup.erase(entityId);
        }
        break;

        default:
            printf("Message Type: %d\n", frame.message_type);
            break;
        }

        pIncomingMsg->Release();
    }
}

bool NetworkGameSystem::OnGameRestored()
{
    const auto res = IGameSystem::OnGameRestored();
    SDK->logger->Info(PLUGIN, "Game restored: We're in the world");
    const auto player = Cyberverse::Utils::GetPlayer();

    // Broken attempts:
    // const auto transform = Cyberverse::Utils::Entity_GetWorldTransform(player);
    // const auto position = Cyberverse::Utils::WorldPosition_ToVector4(transform.Position);
    // SDK->logger->InfoF(PLUGIN, "Player at (%f, %f, %f)", transform.Position.x.Bits, transform.Position.y.Bits,
    // transform.Position.z.Bits);

    const auto position = Cyberverse::Utils::Entity_GetWorldPosition(player);
    SDK->logger->InfoF(PLUGIN, "Player at (%f, %f, %f, %f)", position.X, position.Y, position.Z, position.W);

    PlayerJoinWorld join_packet = {};
    join_packet.position = { position.X, position.Y, position.Z };
    const auto recordId = Cyberverse::Utils::GameObject_GetRecordID(player);
    join_packet.recordId = recordId.value;

    // TODO: Maybe we could manage this singleton access better? But then, the Game's GameSystem Container is the owner
    // of "this"
    Red::GetGameSystem<NetworkGameSystem>()->EnqueueMessage(0, join_packet);

    PlayerAppearance appearance = {};
    auto json = Cyberverse::AppearanceUtils::GetPlayerAppearanceJson(player);
    const auto size = std::min(json.size(), static_cast<size_t>(512));
    appearance.dataLength = static_cast<uint32_t>(size);
    std::memcpy(appearance.data.data(), json.data(), size);
    this->EnqueueMessage(0, appearance);

    m_gameRestored = true;
    return res;
}

void NetworkGameSystem::TrackPlayerPosition(float deltaTime)
{
    m_TimeSinceLastPlayerPositionSync += deltaTime;
    if (m_TimeSinceLastPlayerPositionSync < 0.1f /* update rate*/)
    {
        return;
    }

    m_TimeSinceLastPlayerPositionSync = 0.0f;

    const auto player = Cyberverse::Utils::GetPlayer();
    const auto [X, Y, Z, W] = Cyberverse::Utils::Entity_GetWorldPosition(player);
    const auto orientation = Cyberverse::Utils::Entity_GetWorldOrientation(player);
    const auto [Roll, Pitch, Yaw] = Cyberverse::Utils::Quaternion_ToEulerAngles(orientation);

    const PlayerPositionUpdate position_update = { NextNetworkTick(), {  X, Y, Z }, Yaw};
    this->EnqueueMessage(1, position_update);
}

void NetworkGameSystem::TrackLocomotion(float deltaTime)
{
    static float accum = 0.0f;
    accum += deltaTime;
    if (accum < 0.05f)
        return;
    accum = 0.0f;

    const auto player = Cyberverse::Utils::GetPlayer();
    PlayerLocomotion locomotion = {};
    locomotion.state.stanceId = 0;
    locomotion.state.locomotionId = 0;
    locomotion.state.overlayId = 0;
    locomotion.state.phase = 0;
    this->EnqueueMessage(1, locomotion);
}

void NetworkGameSystem::InterpolatePuppets(const float deltaTime)
{
    for (auto it = m_interpolationData.begin(); it != m_interpolationData.end();)
    {
        auto& entityId = it->first;
        auto& interpolator = it->second;

        const auto interpolationProgress = std::min(1.0f, interpolator.CalcInterpolationProgress(deltaTime));
        const auto targetDestination = Cyberverse::Utils::LerpLocal(interpolator.positionSource, interpolator.positionTarget, interpolationProgress);

        auto angleDirection1 = interpolator.rotationTarget - interpolator.rotationSource;
        auto angleDirection2 = interpolator.rotationSource - interpolator.rotationTarget;

        if (angleDirection1 < 0.0f)
        {
            angleDirection1 += 360.0f;
        }

        if (angleDirection2 < 0.0f)
        {
            angleDirection2 += 360.0f;
        }

        auto actualAngleDistance = 0.0f;
        if (angleDirection1 < angleDirection2)
        {
            actualAngleDistance = angleDirection1;
        } else
        {
            actualAngleDistance = -angleDirection2;
        }

        // TODO: Better interpolation here, e.g. if we go from 5 -> 355°, we should only do 10°, not 350.
        const float targetYaw = interpolator.rotationSource + interpolationProgress * actualAngleDistance;
        const auto targetDestinationVec4 = Cyberverse::Utils::Vector3To4(targetDestination); // TODO: Get rid of this function call
        SDK->logger->TraceF(PLUGIN, "Interpolation Progress: %f, yaw: %f. dest (%f, %f, %f)", interpolationProgress, targetYaw, targetDestinationVec4.X, targetDestinationVec4.Y, targetDestinationVec4.Z);

        SetEntityPosition(entityId, targetDestinationVec4, targetYaw);

        if (interpolationProgress >= 1.0f)
        {
            it = m_interpolationData.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<uint64_t> NetworkGameSystem::GetNetworkedEntityId(const RED4ext::ent::EntityID& entityId) const
{
    for (const auto& [netId, id] : m_networkedEntitiesLookup)
    {
        if (id.hash == entityId.hash)
        {
            return netId;
        }
    }
    return std::nullopt;
}

void NetworkGameSystem::ProcessPendingEntityUpdates()
{
    auto it = m_pendingUpdates.begin();
    while (it != m_pendingUpdates.end())
    {
        auto entity = Cyberverse::Utils::GetDynamicEntity(it->id);
        if (entity.has_value())
        {
            // Wait one frame after the entity becomes available so its components are ready
            if (it->readyFrames++ == 0)
            {
                ++it;
                continue;
            }

            const Red::Entity* basePtr = entity.value();
            auto obj = Red::Handle<Red::GameObject>(reinterpret_cast<Red::GameObject*>(const_cast<Red::Entity*>(basePtr)));
            if (obj && it->appearance.has_value())
            {
                Cyberverse::AppearanceUtils::ApplyAppearance(obj, it->appearance.value());
            }
            if (it->position.has_value() && it->yaw.has_value())
            {
                SetEntityPosition(it->id, it->position.value(), it->yaw.value());
            }
            it = m_pendingUpdates.erase(it);
        }
        else
        {
            if (++it->retries > 10)
            {
                SDK->logger->WarnF(PLUGIN, "Entity %llu still null", it->id.hash);
                it = m_pendingUpdates.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}
