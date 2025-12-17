/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

import lib;
#include "shared/primitive.slang.h"
#include "shared/worklist.slang.h"

#define WORKLIST_DEBUG 0

RWByteAddressBuffer b_primitives;
StructuredBuffer<WorklistItem> b_worklists;
StructuredBuffer<int> b_worklistSizes;
Sampler2D<float4> s_atlas;
RWTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

func msdfMedian(r: float, g: float, b: float) -> float
{ return max(min(r, g), min(max(r, g), b)); }

func sampleGlyphDistance(p: float2, params: Primitive::GlyphParams) -> float
{
	let min_rect = params.atlas_bounds.top_left;
	let max_rect = params.atlas_bounds.bottom_right;
	let atlas_dims = max_rect - min_rect;
	let screen_dims = atlas_dims * params.size;
	let texel_offset = p * (atlas_dims / screen_dims);
	let texel = float2(min_rect.x + texel_offset.x, max_rect.y - texel_offset.y);
	let clamped_texel = clamp(texel, min_rect + 0.5, max_rect - 0.5);
	uint w, h;
	s_atlas.GetDimensions(w, h);
	let uv = clamped_texel / float2(w, h);
	let s = s_atlas.SampleLevel(uv, 0).rgb;
	let sd = msdfMedian(s.r, s.g, s.b);
	let dist = (0.5 - sd) * 2.0;
	let dist_outside = length(texel - clamped_texel);
	return (dist + dist_outside) * params.size;
}

[shader("compute")]
[numthreads(16, 16)]
void computeMain(int2 threadId: SV_DispatchThreadID)
{
	if (any(threadId >= ViewportSize)) return;
	let tile = threadId / TILE_SIZE;
	let tileIdx = tile.y * TileBound.x + tile.x;
	let worklistOffset = tileIdx * WORKLIST_MAX_SIZE;
	let worklistSize = b_worklistSizes[tileIdx];
	if (worklistSize <= 0) return;

	let self_pos = float2(threadId) + 0.5;
	var color = i_target.Load(threadId);
	color = float4(srgbDecode(color.rgb), color.a);

	for (int i = 0; i < worklistSize; i += 1) {
		let item = b_worklists[worklistOffset + i];
		let primitive = readPrimitive(b_primitives, item.primitive_idx);
		let params = readPrimitiveParams(b_primitives, item.primitive_idx);
		var coverage = 0.0;
		let center = self_pos - primitive.position;
		switch (primitive.type) {
		case Primitive::Type::Circle:
			let circleParams = reinterpret<Primitive::CircleParams>(params);
			coverage = sdf::aa(sdf::circle(center, circleParams.radius));
			break;
		case Primitive::Type::Rect:
			let rectParams = reinterpret<Primitive::RectParams>(params);
			coverage = sdf::aa(sdf::rect(center, rectParams.size));
			break;
		case Primitive::Type::Glyph:
			let glyphParams = reinterpret<Primitive::GlyphParams>(params);
			coverage = sdf::aa(sampleGlyphDistance(center, glyphParams));
			break;
		}
		color = blend(color, float4(primitive.color.rgb, primitive.color.a * coverage));
	}

	color = float4(srgbEncode(color.rgb), color.a);
#if WORKLIST_DEBUG
	color.r += 0.08 * worklistSize;
#endif
	i_target.Store(threadId, color);
}
