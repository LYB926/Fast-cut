# Fast Cut

Fast Cut 是一款小巧的、基于 GTK 的辅助工具，它封装了 ffmpeg 命令，用于从视频中剪辑片段，并使用 hevc_nvenc/hevc_amf/hevc_qsv 进行重新编码。

Fast Cut is a small GTK-based helper that wraps an ffmpeg command for cutting a segment from a video and re-encoding it with `hevc_nvenc`.

## Features

- 将视频文件拖拽到窗口上，或通过“打开”对话框进行选择。
- 输入开始和结束时间戳（例如 `00:03:58` 和 `00:04:07`）。
- 选择一个 NVENC 预设（`p1`-`p7`，默认为 `p5`）。
- 调整建议的输出路径（`*_hevc.mp4`）。
- 在后台启动 ffmpeg，并在日志面板中查看 stdout/stderr 输出。

---

- Drag a video file onto the window or choose it via the open dialog.
- Enter start and end timestamps (for example `00:03:58` and `00:04:07`).
- Pick an NVENC preset (`p1`-`p7`, default is `p5`).
- Adjust the suggested output path (`*_hevc.mp4`).
- Launch ffmpeg in the background and review stdout/stderr in the log panel.

## Installation and How It Works

1. 下载FFmpeg的预编译版本，例如 https://github.com/BtbN/FFmpeg-Builds/releases 的 ffmpeg-master-latest-win64-gpl-shared.zip，解压缩到某个目录并且将 `.../ffmpeg/bin` 目录添加到环境变量 PATH。
2. 在 https://github.com/LYB926/Fast-cut/releases/tag/1.0 下载软件即可使用。

1.  Download a pre-compiled version of FFmpeg, for example, `ffmpeg-master-latest-win64-gpl-shared.zip` from https://github.com/BtbN/FFmpeg-Builds/releases. Extract it to a directory and add the `.../ffmpeg/bin` directory to your PATH environment variable.
2.  Download the software from https://github.com/LYB926/Fast-cut/releases/tag/1.0 to use it.

---

这个软件的原理是执行如下命令：
```bash
ffmpeg -ss HH:MM:SS -to HH:MM:SS -i "Video Name.mp4" -c:v hevc_nvenc -preset p5 "Video Name_hevc.mp4"
```
命令的构建部分在 `build_ffmpeg_argv` 函数内，因此，只要你想或需要，可以通过简单修改此函数来使用除 `hevc_nvenc`(NVIDIA) 以外的Codec，例如 `hevc_qsv`(Intel) 和 `hevc_amf`(AMD)。

This software works by executing the following command:
```bash
ffmpeg -ss HH:MM:SS -to HH:MM:SS -i "Video Name.mp4" -c:v hevc_nvenc -preset p5 "Video Name_hevc.mp4"
```
The command construction part is located in the `build_ffmpeg_argv` function. Therefore, if you want or need to, you can use codecs other than `hevc_nvenc` (NVIDIA) by simply modifying this function, such as `hevc_qsv` (Intel) and `hevc_amf` (AMD).

---

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

