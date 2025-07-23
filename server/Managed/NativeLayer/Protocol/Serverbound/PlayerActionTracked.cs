using System.Runtime.InteropServices;
using Cyberverse.Server.NativeLayer.Protocol.Common;

namespace Cyberverse.Server.NativeLayer.Protocol.Serverbound;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct PlayerActionTracked
{
    // True when the triggering button is pressed
    public bool buttonState;
    // Padding to align to 8 bytes (C++ will pad after bool)
    private byte pad0, pad1, pad2, pad3, pad4, pad5, pad6;
    // Tick counter relative to the connection start
    public ulong networkTick;
    public EPlayerAction action;
    public Vector3 worldTransform;
}
