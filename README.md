# Niseyuki

Niseyuki is a native Qt6 desktop front-end for anime-focused encoding workflows built around ffmpeg. It wraps per-job settings, preview controls, and integrates bundled assets so the final encodes are Telegram-ready without external scripts.

## Building

1. Configure (example for Visual Studio generator):
   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.7.2\msvc2019_64"
   ```
2. Build the chosen configuration:
   ```powershell
   cmake --build build --config Release
   ```

The executable will be available under `build/<config>/niseyuki.exe`.

## Packaging (Windows)

After building, run the helper script to produce a zip that includes Qt runtime files and optional ffmpeg binaries:

```powershell
pwsh scripts/package_windows.ps1 `
    -BuildDir build `
    -Configuration Release `
    -QtBinDir "C:\Qt\6.7.2\msvc2019_64\bin" `
    -FfmpegDir "C:\tools\ffmpeg\bin" `
    -OutputZip niseyuki-windows.zip
```

The script will:
- run `cmake --install` into a staging folder
- invoke `windeployqt` to copy Qt6 runtime dependencies
- optionally copy `ffmpeg.exe`, `ffprobe.exe`, and `ffplay.exe` from the provided directory into `ffmpeg\bin`
- compress the staged directory into the requested zip file

If `-QtBinDir` is omitted the script tries to locate `windeployqt.exe` on the current `PATH`. `-FfmpegDir` can be skipped when you want to distribute without ffmpeg binaries.

## Bundled ffmpeg lookup

At runtime Niseyuki resolves ffmpeg/ffprobe in this order:
1. `NISEYUKI_FFMPEG` / `NISEYUKI_FFPROBE` environment variables (absolute paths)
2. `ffmpeg/bin/ffmpeg(.exe)` and `ffmpeg/bin/ffprobe(.exe)` next to the executable
3. `ffmpeg/` inside the application directory
4. The system `PATH`

Place your preferred ffmpeg build under `ffmpeg/bin` beside `niseyuki.exe` (or set the environment variables) to keep the application self-contained.

## Current status

- Queue UI now captures job settings including renderer choice, resize, audio codec/bitrate, Telegram mode, etc.
- Encoding pipeline re-encodes with the requested codec/preset, applies libass subtitles, resize filters, CRF/CQ, and updates progress based on `-progress` output.
- Features not yet implemented (intro/outro stitching, logo overlay, additional subtitle muxing, thumbnail injection) are gracefully logged as warnings and skipped.

