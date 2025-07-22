using System.Runtime.InteropServices;
using Cyberverse.Server.NativeLayer.Protocol.Common;

namespace Cyberverse.Server.NativeLayer.Protocol.Serverbound;


[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct PlayerPositionUpdate
{
    // Tick counter relative to the connection start
    public ulong networkTick;
    public Vector3 worldTransform;
    public float yaw;
}