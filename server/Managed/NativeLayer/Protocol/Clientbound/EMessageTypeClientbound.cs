﻿namespace Cyberverse.Server.NativeLayer.Protocol.Clientbound;

public enum EMessageTypeClientbound: ushort
{
    InitAuthResult = 0,
    SpawnEntity = 1,
    TeleportEntity = 2,
    DestroyEntity = 3,
    EquipItemEntity = 4,
    ApplyAppearance = 5,
    UpdateLocomotion = 6,
}