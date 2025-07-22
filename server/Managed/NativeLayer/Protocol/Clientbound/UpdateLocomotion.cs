using System.Runtime.InteropServices;
using Cyberverse.Server.NativeLayer.Protocol.Common;

namespace Cyberverse.Server.NativeLayer.Protocol.Clientbound;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct UpdateLocomotion : IClientBoundPacket
{
    public ulong networkedEntityId;
    public LocomotionState state;

    public EMessageTypeClientbound GetMessageType() => EMessageTypeClientbound.UpdateLocomotion;
}
