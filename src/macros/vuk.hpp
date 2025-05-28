/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

macros/vuk.hpp:
Redefines macros used by vuk. Importing playnote.lib.vulkan is required.
*/

#ifndef PLAYNOTE_MACROS_VUK_HPP
#define PLAYNOTE_MACROS_VUK_HPP

// from vuk/RenderGraph.hpp
#define VUK_IA(access) vk::Arg<vk::ImageAttachment, access, vk::tag_type<__COUNTER__>>
#define VUK_BA(access) vk::Arg<vk::Buffer, access, vk::tag_type<__COUNTER__>>

#endif
