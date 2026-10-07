[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
if (-not ('GeneralsGPT.DisplayModes' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace GeneralsGPT {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DisplayMode {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string DeviceName;
        public ushort SpecVersion, DriverVersion, Size, DriverExtra;
        public uint Fields;
        public int PositionX, PositionY;
        public uint Orientation, FixedOutput;
        public short Color, Duplex, YResolution, TTOption, Collate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string FormName;
        public ushort LogPixels;
        public uint BitsPerPixel, Width, Height, Flags, Frequency;
        public uint ICMMethod, ICMIntent, MediaType, DitherType, Reserved1, Reserved2;
        public uint PanningWidth, PanningHeight;
    }
    public static class DisplayModes {
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool EnumDisplaySettings(string device, int index, ref DisplayMode mode);
    }
}
'@
}

for ($modeIndex = 0; ; $modeIndex++) {
    $displayMode = New-Object GeneralsGPT.DisplayMode
    $displayMode.Size = [Runtime.InteropServices.Marshal]::SizeOf($displayMode)
    if (-not [GeneralsGPT.DisplayModes]::EnumDisplaySettings($null, $modeIndex, [ref]$displayMode)) {
        if ($modeIndex -eq 0) { throw 'Display mode enumeration is unavailable in this session. Do not interpret this as an unsupported resolution.' }
        break
    }
    if ($displayMode.BitsPerPixel -ge 24) {
        [PSCustomObject]@{
            Width = $displayMode.Width
            Height = $displayMode.Height
            BitsPerPixel = $displayMode.BitsPerPixel
            RefreshRate = $displayMode.Frequency
        }
    }
}
