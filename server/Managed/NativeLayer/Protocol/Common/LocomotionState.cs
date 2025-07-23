using System.Runtime.InteropServices;

namespace Cyberverse.Server.NativeLayer.Protocol.Common;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct LocomotionState
{
    public ushort stanceId;
    public ushort locomotionId;
    public ushort overlayId;
    public ushort phase;
}
