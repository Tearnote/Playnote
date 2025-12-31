/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
except according to those terms.
*/

#include "gpu/shaders.hpp"

namespace playnote::gpu {

namespace detail {
#include "spv/worklist_gen.slang.spv.h"
#include "spv/worklist_sort.slang.spv.h"
#include "spv/draw_all.slang.spv.h"
#include "spv/imgui.slang.spv.h"
};

vector<uint32_t> const worklist_gen_spv(detail::worklist_gen_spv, detail::worklist_gen_spv + detail::worklist_gen_spv_sizeInBytes / sizeof(uint32_t));
vector<uint32_t> const worklist_sort_spv(detail::worklist_sort_spv, detail::worklist_sort_spv + detail::worklist_sort_spv_sizeInBytes / sizeof(uint32_t));
vector<uint32_t> const draw_all_spv(detail::draw_all_spv, detail::draw_all_spv + detail::draw_all_spv_sizeInBytes / sizeof(uint32_t));
vector<uint32_t> const imgui_spv(detail::imgui_spv, detail::imgui_spv + detail::imgui_spv_sizeInBytes / sizeof(uint32_t));

}
