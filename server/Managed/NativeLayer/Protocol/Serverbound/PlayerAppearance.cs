using System.Runtime.InteropServices;
using Cyberverse.Server.NativeLayer.Protocol.Common;

namespace Cyberverse.Server.NativeLayer.Protocol.Serverbound;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct PlayerAppearance
{
    public uint dataLength;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
    public byte[] data;
}
