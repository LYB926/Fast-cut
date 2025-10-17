# Fast Cut

Fast Cut is a small GTK-based helper that wraps an ffmpeg command for cutting a segment from a video and re-encoding it with `hevc_nvenc`.

## Features

- Drag a video file onto the window or choose it via the open dialog.
- Enter start and end timestamps (for example `00:03:58` and `00:04:07`).
- Pick an NVENC preset (`p1`-`p7`, default is `p5`).
- Adjust the suggested output path (`*_hevc.mp4`).
- Launch ffmpeg in the background and review stdout/stderr in the log panel.

## Requirements

- MinGW toolchain with `gcc`.
- GTK+ 3 runtime and development files accessible via `pkg-config`.
- `ffmpeg` available in `PATH`, with NVENC support (NVIDIA GPU required).

## Build

```pwsh
.\build.bat
```

The script stores the executable at `build\fast_cut.exe`.

## Run

```pwsh
.\build\fast_cut.exe
```

Make sure GPU drivers and ffmpeg NVENC binaries are installed so the `hevc_nvenc` encoder can be used successfully.
