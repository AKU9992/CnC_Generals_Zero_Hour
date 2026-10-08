# Windows 11 startup work

Status: source patches awaiting a build. The user confirmed that the installed
Zero Hour starts after renaming the bundled debugger DLL. The 3440 x 1440
configuration is applied. On 2026-10-06, the user confirmed that the requested
menu, skirmish, image proportions and cursor/interface checks work correctly.
Applies to both `Generals` and `GeneralsMD` (Zero Hour).

## Problems addressed

- Fullscreen startup previously retried the requested resolution at 16 bits after
  a 32-bit failure. Both attempts can fail when the monitor does not expose that
  resolution. Startup now tries the current desktop resolution at 32 bits between
  those attempts. A successful requested mode and explicit windowed mode are
  unchanged. The fallback updates the display and in-memory global resolution;
  it does not directly rewrite `Options.ini`.
- The Direct3D wrapper ignored failure to select a fullscreen color/depth format
  or retrieve the windowed desktop format. It now stops that device attempt, so
  startup can try the next mode without using invalid or stale format values.
- An empty render-device table was protected only by debug assertions. Release
  builds now return failure before indexing it, including invalid device indices.
- The executable path was used without checking failure or truncation. Startup
  now reports those failures and a failed working-directory change. Keeping the
  final separator also fixes installation in a drive root (`E:\`, rather than
  the drive-relative `E:`).
- Window-class registration and window creation failures now produce a message
  and a nonzero exit code. The outer exception handler also shows a message
  instead of silently returning success.

These are code defects and recovery improvements, not a diagnosis of a specific
installed game's failure. The edition, exact error and reproduction are still
needed. A crash before `WinMain` (for example, a missing imported DLL) cannot be
reported by the new handler.

## Build and verification limits

The upstream [README](../README.md) documents legacy Visual C++ projects and
missing third-party dependencies. No C++ compiler was available on PATH or in
the standard Visual Studio installation directories during this work. The
dependency folders checked for STLport, Miles, GameSpy and zlib contain only
placeholder `.gitignore` files. A full build and executable test therefore remain
pending; modifying these sources does not change an installed retail binary.

Source diffs have been reviewed and checked with `git diff --check`.

## Required runtime checks after building

Run the following for both games with owned game data on Windows 11:

| Case | Expected result |
| --- | --- |
| Supported requested fullscreen mode | Original mode starts; no desktop retry |
| Unsupported saved resolution, supported desktop mode | Starts fullscreen at desktop resolution, 32 bits |
| Desktop mode unavailable or desktop retry fails | Original requested resolution is retried at 16 bits |
| All render-device attempts fail | Existing Direct3D failure is reported |
| Explicit `-win` with a valid resolution | Windowed mode starts without fullscreen retry |
| No usable adapter or invalid device index | Device initialization fails without table access |
| Window class or window creation fails | Startup error is visible; exit code is nonzero |
| Executable path read fails or exceeds the buffer | Startup error is visible; no truncated path is used |
| Executable in a drive root, launched with another working directory | Data is loaded from the executable's drive root |
| Existing options and saves | Still readable; confirm normal menu, skirmish and exit |

Check the desktop fallback with display scaling enabled and with more than one
monitor. It currently uses the current Windows display and renderer device 0,
matching the existing renderer's startup selection. Multi-adapter selection is
outside this patch.

## API references

- [EnumDisplaySettingsA](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsa):
  `ENUM_CURRENT_SETTINGS` returns the current display mode in physical pixels.
- [GetModuleFileNameA](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamea):
  failure returns zero; a result equal to the buffer capacity indicates truncation.

## Local installation diagnosis (2026-10-06)

The supplied installation contains Generals in `E:\C&C ZH GPTMOD\CaCG` and
Zero Hour in `E:\C&C ZH GPTMOD\CaCGZH`. Executable version metadata is absent,
so the installed game edition/patch level has not been established.

with a current resolution of 3440 x 1440. Zero Hour's existing options request
1280 x 1024. The local `d3d8.dll` identifies itself as GenTool. A bundled
`dbghelp.dll` identifies itself as Windows 2000 version 5.00.2195.1.

The existing January 17, 2026 crash report records a null-pointer access violation
in `GameEngine::update`; it is historical and does not diagnose the current
startup failure.

With filesystem approval, only Zero Hour's bundled debugger DLL was renamed:

```text
E:\C&C ZH GPTMOD\CaCGZH\dbghelp.dll
    -> dbghelp.dll.win11-backup
```

The original file is preserved. This matches the DLL-renaming compatibility fix
listed by the [GenPatcher maintainer](https://legi.cc/genpatcher/). No game binary,
GenTool DLL or saved game was changed. The user subsequently confirmed successful
startup. This confirms the installed-copy workaround, not the unbuilt C++ changes.

To roll back with the game closed:

```powershell
Rename-Item -LiteralPath 'E:\C&C ZH GPTMOD\CaCGZH\dbghelp.dll.win11-backup' -NewName 'dbghelp.dll'
```

If normal launch still fails, compare an explicit windowed launch (using the
same executable, `-win -xres 1280 -yres 720`) and collect the exact error plus a
fresh crash report. Do not treat an old report as evidence of that test.

## High-resolution changes

Both source trees now expose all enumerated modes of at least 800 x 600 and
24-bit color, removing the 4:3 aspect-ratio restriction. Thus 3440 x 1440
(the local monitor) and 3840 x 2160 (4K UHD) can appear in the options menu when
the renderer/driver enumerates them. No unsupported modes are fabricated.
The count and description methods share the same predicate. The options menu
also uses the dimensions of its actual fallback entry if a saved mode is missing,
fixing an existing assignment of 600 to the width instead of the height.

The Generals startup fallback now has definitions for its minimum dimensions;
these were missing in the first unbuilt source patch. The minimum-height constant
is spelled consistently in both trees.

### Installed game configuration

`tools/Set-ZeroHourResolution.ps1` changes only the Resolution entry and makes
a unique adjacent backup first. It preserves unrelated bytes and supports
restoring a chosen backup. Close the game before each invocation.

```powershell
.\tools\Set-ZeroHourResolution.ps1 -Profile UltraWide  # 3440 x 1440
.\tools\Set-ZeroHourResolution.ps1 -Profile FourK      # 3840 x 2160
.\tools\Set-ZeroHourResolution.ps1 -Profile Restore -BackupPath 'full path returned by a previous run'
```

UltraWide was applied to the local Zero Hour Options.ini with filesystem approval.
The original is preserved at:

```text
```

The FourK profile is available but has not been applied to the installed game.
The current panel's native resolution is 3440 x 1440. Fullscreen 4K requires
an appropriate display or a higher-resolution mode offered by the driver.
`tools/Get-DisplayModes.ps1` can list those modes from an interactive Windows
session. Enumeration returned no modes in the agent session, so driver support
for 3840 x 2160 remains unknown; it is not established as unsupported.

### Verification

Run `tools/Test-ResolutionProfiles.ps1`. Its file-based checks passed for both
profiles, byte preservation, original backup, restore, WhatIf, a missing entry,
duplicate-entry rejection and unsupported-encoding rejection. These tests do
not execute the renderer or validate the C++ build.

The user confirmed successful installed-copy testing at 3440 x 1440 on
2026-10-06 after being asked to check the menu, a short skirmish, image proportions
and cursor/interface alignment. This is user-reported runtime verification;
the agent did not inspect rendered frames. The 3840 x 2160 runtime test and a
build/test of the modified C++ executable remain pending. The installed-copy
checkpoint for proceeding to texture work is complete.
