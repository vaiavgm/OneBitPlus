# Cheatsheet: commands on the local Windows machine

Local paths (always use these full paths):

| What | Path |
|---|---|
| This repo | `E:\iPlug2\Projects\VaiaOneBitPlus` |
| iPlug2 root (the project must sit three folders inside it) | `E:\iPlug2` |
| VST3 SDK (iPlug2 dependency) | `E:\iPlug2\Dependencies\IPlug\VST3_SDK` |
| Build output | `E:\iPlug2\Projects\VaiaOneBitPlus\build-win\vst3\x64\<Configuration>\VaiaOneBitPlus.vst3` |

The project uses toolset **v145** (Visual Studio 2026 / MSVC 14.5), x64.

## Get the latest code

```powershell
git -C "E:\iPlug2\Projects\VaiaOneBitPlus" fetch origin
git -C "E:\iPlug2\Projects\VaiaOneBitPlus" checkout feature/clean-up
git -C "E:\iPlug2\Projects\VaiaOneBitPlus" pull origin feature/clean-up
```

## Build the VST3 only (x64)

PowerShell. It finds MSBuild by itself, so no Visual Studio path is needed:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msbuild "E:\iPlug2\Projects\VaiaOneBitPlus\projects\VaiaOneBitPlus-vst3.vcxproj" /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=E:\iPlug2\Projects\VaiaOneBitPlus\" /m /v:minimal
```

- `Configuration` is `Release` or `Debug` (`Tracer` exists too). Use `Debug` when you want to attach a debugger.
- `SolutionDir` **must end with a backslash**: the project files use it for the output folder and the pre/post-build scripts. Without it the build fails with a "scripts\prebuild-win.bat not found" error.
- Add `/t:Rebuild` for a clean rebuild.
- The same thing in Visual Studio: open `E:\iPlug2\Projects\VaiaOneBitPlus\VaiaOneBitPlus.sln`, pick `Release | x64`, right-click the project `VaiaOneBitPlus-vst3` and choose Build.

## Install it for REAPER

The post-build step does not copy anything, so copy the file yourself (needs an elevated PowerShell for this folder):

```powershell
Copy-Item "E:\iPlug2\Projects\VaiaOneBitPlus\build-win\vst3\x64\Release\VaiaOneBitPlus.vst3" "C:\Program Files\Common Files\VST3\VaiaOneBitPlus.vst3" -Force
```

REAPER must not have the plugin loaded while copying (close the FX window or REAPER). Then rescan in REAPER (Preferences > Plug-ins > VST > Re-scan) if it doesn't show up.

## Diagnostics while testing in REAPER

`ONEBIT_DIAGNOSTICS` in `src\Debug.h` is 0 by default (set it to 1 and rebuild to get the log). Run **DebugView** (Sysinternals) as administrator with "Capture Win32" on, filter for `OneBitPlus:`. The first line after loading is `OneBitPlus: OnReset ... build=<date time> dspVersion=1`: use it to check that you are running the build you just made. What each line means: `docs\BUILD.md`, section "Diagnostics".

## If the build fails

- "Cannot open include file" for VST3 or IGraphics headers: the iPlug2 dependencies are missing. They live in `E:\iPlug2\Dependencies` (see `E:\iPlug2\Dependencies\README.md`, and `E:\iPlug2\Dependencies\IPlug\README.md` for the SDKs such as `VST3_SDK`).
- `Oscillator.h(68): error C2039: 'sin': is not a member of 'std'` / `PI` not found: iPlug2's `Oscillator.h` does not include `<cmath>` and `IPlugConstants.h`; newer MSVC libraries no longer include them implicitly. Fixed in `src\SampleOscillator.h`, `src\VaiaOscillator.h` and `src\ParamInit.h` by including them first (pull the latest).
- "Platform toolset v145 not found": install the current Visual Studio C++ workload, or change `PlatformToolset` in the project files.
- Anything in `src\SamplesPanel.h` or `src\VaiaOneBitPlus.cpp`: these were only compile-checked with clang on Linux so far. Send the first error lines.
