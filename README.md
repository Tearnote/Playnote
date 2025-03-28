# Playnote

A [BMS](https://en.wikipedia.org/wiki/Be-Music_Source) player hobby project, written in modern C++. Work in progress.

## Initial goals

- Loading songs directly from `.zip` archives
- Smallest possible audio latency by interfacing with [WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi) and [PipeWire](https://pipewire.org/) directly with a custom audio playback engine
- Threaded input handling for millisecond-accurate timestamps regardless of framerate
- [LR2-compatible](https://github.com/seraxis/lr2oraja-endlessdream) gauges and timing windows
