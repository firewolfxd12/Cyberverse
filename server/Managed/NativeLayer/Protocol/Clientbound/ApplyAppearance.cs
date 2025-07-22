using System.Runtime.InteropServices;
using Cyberverse.Server.NativeLayer.Protocol.Common;

namespace Cyberverse.Server.NativeLayer.Protocol.Clientbound;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct ApplyAppearance : IClientBoundPacket
{
    public ulong networkedEntityId;
    public uint dataLength;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
    public byte[] data;

    public EMessageTypeClientbound GetMessageType() => EMessageTypeClientbound.ApplyAppearance;
}
