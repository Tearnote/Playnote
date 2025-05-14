# Playnote

A hobby project for a stable, modern and intuitive [BMS](https://en.wikipedia.org/wiki/Be-Music_Source) player, written in modern C++.
Work in progress.

## Milestones

- [ ] `0.0.1`: A usable non-interactive player of BMS songs.
  - Loads a BMS chart via a command-line argument
  - Plays it back directly via [PipeWire](https://pipewire.org/) with minimal latency
  - Basic Imgui playback controls

## [Dependencies](./cmake/Dependencies.cmake)

Required to be installed locally:

- Linux (temporarily)
- [Clang](https://clang.llvm.org/) ([Apache](https://github.com/llvm/llvm-project/blob/main/LICENSE.TXT))
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) (with [glslc](https://github.com/google/shaderc) and [glslangValidator](https://github.com/KhronosGroup/glslang)) ([various licenses](https://vulkan.lunarg.com/software/license/vulkan-1.4.313.0-linux-license-summary.txt))
- [glfw](https://www.glfw.org/) ([zlib](https://www.glfw.org/license.html))
- [libsndfile](https://libsndfile.github.io/libsndfile/) ([LGPL](https://libsndfile.github.io/libsndfile/#licensing))
- [libsamplerate](https://libsndfile.github.io/libsamplerate/) ([BSD](https://libsndfile.github.io/libsamplerate/license.html))
- [libpipewire](https://pipewire.org/) ([MIT](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/COPYING))

Retrieved via [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html):

- [libassert](https://github.com/jeremy-rifkin/libassert) ([MIT](https://github.com/jeremy-rifkin/libassert?tab=MIT-1-ov-file#readme))
- [mio](https://github.com/vimpunk/mio) ([MIT](https://github.com/vimpunk/mio?tab=MIT-1-ov-file#readme))
- [unordered_dense](https://github.com/martinus/unordered_dense) ([MIT](https://github.com/martinus/unordered_dense?tab=MIT-1-ov-file#readme))
- [quill](https://github.com/odygrd/quill) ([MIT](https://github.com/odygrd/quill?tab=MIT-1-ov-file#readme))
- [volk](https://github.com/zeux/volk) ([MIT](https://github.com/zeux/volk?tab=MIT-1-ov-file#readme))
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) ([MIT](https://github.com/charles-lunarg/vk-bootstrap?tab=MIT-1-ov-file#readme))
- [vuk](https://github.com/martty/vuk) ([MIT](https://github.com/martty/vuk?tab=MIT-1-ov-file#readme))
- [tracy](https://github.com/wolfpld/tracy) ([BSD](https://github.com/wolfpld/tracy?tab=License-1-ov-file#readme))
