# Playnote

A [BMS](https://en.wikipedia.org/wiki/Be-Music_Source) player for Windows and Linux, with a focus on correctness and user experience.

## Why another BMS player from scratch?

Because our community is long overdue for an upgrade. The players we use right now are undoubtedly impressive and valued, but there are several important improvements that can be made, some of which would require a fundamental redesign. These include:

- Loading charts directly from archives,
- Chart volume normalization,
- Ultra-low audio latency by default,
- A frictionless newbie experience,
- Advanced practice modes,
- Framerate-independent input processing,
- Stable performance and memory usage,
- Charset detection,
- Background processing of tasks like preview generation and library refresh.

## Can I contribute?

Playnote is a personal passion project, written in bleeding-edge C++ that is rather different from most code out there. Issue reports are appreciated, but any non-trivial pull request I would most likely refactor of rewrite before merging to maintain style and vision. If you are okay with that, please go ahead.

## [Dependencies](./cmake/Dependencies.cmake)

Required to be installed locally:

- Linux (for now)
- [Clang](https://clang.llvm.org/) and [lld](https://lld.llvm.org/) ([Apache](https://github.com/llvm/llvm-project/blob/main/LICENSE.TXT))
- [Boost](https://www.boost.org/) ([Boost](https://www.boost.org/doc/user-guide/bsl.html))
- [OpenSSL](https://openssl-library.org/) ([Apache](https://github.com/openssl/openssl?tab=Apache-2.0-1-ov-file#readme))
- [ICU](https://icu.unicode.org/) ([Unicode](https://github.com/unicode-org/icu/blob/main/LICENSE))
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) (with [glslc](https://github.com/google/shaderc) and [glslangValidator](https://github.com/KhronosGroup/glslang)) ([various licenses](https://vulkan.lunarg.com/software/license/vulkan-1.4.313.0-linux-license-summary.txt))
- [glfw](https://www.glfw.org/) ([zlib](https://www.glfw.org/license.html))
- [FFmpeg](https://ffmpeg.org/) ([LGPL](https://git.ffmpeg.org/gitweb/ffmpeg.git/blob/HEAD:/LICENSE.md))
- [libpipewire](https://pipewire.org/) ([MIT](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/COPYING))

Retrieved via CMake [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html):

- [libassert](https://github.com/jeremy-rifkin/libassert) ([MIT](https://github.com/jeremy-rifkin/libassert?tab=MIT-1-ov-file#readme))
- [mio](https://github.com/vimpunk/mio) ([MIT](https://github.com/vimpunk/mio?tab=MIT-1-ov-file#readme))
- [quill](https://github.com/odygrd/quill) ([MIT](https://github.com/odygrd/quill?tab=MIT-1-ov-file#readme))
- [volk](https://github.com/zeux/volk) ([MIT](https://github.com/zeux/volk?tab=MIT-1-ov-file#readme))
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) ([MIT](https://github.com/charles-lunarg/vk-bootstrap?tab=MIT-1-ov-file#readme))
- [vuk](https://github.com/martty/vuk) ([MIT](https://github.com/martty/vuk?tab=MIT-1-ov-file#readme))
- [tracy](https://github.com/wolfpld/tracy) ([BSD](https://github.com/wolfpld/tracy?tab=License-1-ov-file#readme))
- [cpp-channel](https://blog.andreiavram.ro/cpp-channel-thread-safe-container-share-data-threads/) ([MIT](https://github.com/andreiavrammsd/cpp-channel?tab=MIT-1-ov-file))
- [Dear ImGui](https://github.com/ocornut/imgui) ([MIT](https://github.com/ocornut/imgui?tab=MIT-1-ov-file#readme))
