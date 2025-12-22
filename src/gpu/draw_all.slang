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
#include "shared/config.slang.h"

#define WORKLIST_DEBUG 0
#define ATLAS_DEBUG 0

RWByteAddressBuffer b_primitives;
StructuredBuffer<WorklistItem> b_worklists;
StructuredBuffer<int> b_worklistSizes;
Sampler2D<float4> s_staticAtlas;
Sampler2D<float4> s_dynamicAtlas;
RWTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let SubpixelRenderingRaw = 0u;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;
static let SubpixelRendering = SubpixelRenderingMode(SubpixelRenderingRaw);

static let SubpixelLayoutRGB: float2[] = {
	{0.166, 0.5},
	{0.500, 0.5},
	{0.833, 0.5},
};

groupshared var worklist: WorklistItem[WORKLIST_MAX_SIZE];

func msdfMedian(r: float, g: float, b: float) -> float
{ return max(min(r, g), min(max(r, g), b)); }

func sampleGlyphDistance(p: float2, params: Primitive::GlyphParams) -> float
{
	let minRect = params.atlas_bounds.top_left;
	let maxRect = params.atlas_bounds.bottom_right;
	let atlasDims = maxRect - minRect;
	let screenDims = atlasDims * params.size;
	let texelOffset = p * (atlasDims / screenDims);
	let texel = float2(minRect.x + texelOffset.x, maxRect.y - texelOffset.y);
	let clampedTexel = clamp(texel, minRect + 0.5, maxRect - 0.5);
	var w: uint;
	var h: uint;
	if (params.page == 0)
		s_staticAtlas.GetDimensions(w, h);
	else
		s_dynamicAtlas.GetDimensions(w, h);
	let uv = clampedTexel / float2(w, h);
	var s: float3;
	if (params.page == 0)
		s = s_staticAtlas.SampleLevel(uv, 0).rgb;
	else
		s = s_dynamicAtlas.SampleLevel(uv, 0).rgb;
	let sd = msdfMedian(s.r, s.g, s.b);
	let dist = (0.5 - sd) * 8.0;
	let distOutside = length(texel - clampedTexel);
	return (dist + distOutside) * params.size;
}

func primitiveCoverage(primitive: Primitive, params: int[8], at: float2) -> float
{
	let center = at - primitive.position;
	switch (primitive.type) {
	case Primitive::Type::Circle:
		let circleParams = reinterpret<Primitive::CircleParams>(params);
		return sdf::aa(sdf::circle(center, circleParams.radius));
	case Primitive::Type::Rect:
		let rectParams = reinterpret<Primitive::RectParams>(params);
		return sdf::aa(sdf::rect(center, rectParams.size));
	case Primitive::Type::Glyph:
		let glyphParams = reinterpret<Primitive::GlyphParams>(params);
		return sdf::aa(sampleGlyphDistance(center, glyphParams));
	default: return 0.0;
	}
}

[shader("compute")]
[numthreads(TILE_SIZE, TILE_SIZE)]
void computeMain(int2 threadId: SV_DispatchThreadID, uint3 groupThreadId: SV_GroupThreadID)
{
	// Retrieve tile worklist from buffer
	let tile = threadId / TILE_SIZE;
	let tileIdx = tile.y * TileBound.x + tile.x;
	let worklistSize = b_worklistSizes[tileIdx];
	if (worklistSize <= 0) return;
	let worklistOffset = tileIdx * WORKLIST_MAX_SIZE;
	let flatGroupThreadId = groupThreadId.y * TILE_SIZE + groupThreadId.x;
	if (flatGroupThreadId < worklistSize)
		worklist[flatGroupThreadId] = b_worklists[worklistOffset + flatGroupThreadId];
	GroupMemoryBarrierWithGroupSync();

	if (any(threadId >= ViewportSize)) return;
	let selfPos = float2(threadId) + 0.5;
	var color = i_target.Load(threadId);
	color = float4(srgbDecode(color.rgb), color.a);

	if (SubpixelRendering == SubpixelRenderingMode::None) {
		for (int i = 0; i < worklistSize; i += 1) {
			let item = worklist[i];
			let primitive = readPrimitive(b_primitives, item.primitive_idx);
			let params = readPrimitiveParams(b_primitives, item.primitive_idx);
			let coverage = primitiveCoverage(primitive, params, selfPos);
			color = blend(color, float4(primitive.color.rgb
#if ATLAS_DEBUG
			* (primitive.type == Primitive::Type::Glyph && reinterpret<Primitive::GlyphParams>(params).page == 1? float3(1.0, 0.5, 0.5) : float3(1.0, 1.0, 1.0))
#endif
			, primitive.color.a * coverage));
		}
	} else {
		var results: float2[3] = {};
		for (int j = 0; j < 3; j += 1) {
			for (int i = 0; i < worklistSize; i += 1) {
				let item = worklist[i];
				let primitive = readPrimitive(b_primitives, item.primitive_idx);
				let params = readPrimitiveParams(b_primitives, item.primitive_idx);
				let coverage = primitiveCoverage(primitive, params, selfPos + (SubpixelLayoutRGB[j] - float2(0.5, 0.5)));
				let colorComponent = primitive.color[j];
				results[j] = blend(results[j], float2(colorComponent
#if ATLAS_DEBUG
				* (j == 0 && primitive.type == Primitive::Type::Glyph && reinterpret<Primitive::GlyphParams>(params).page == 1? 1.0 : 0.5)
#endif
				, primitive.color.a * coverage));
			}
		}
		color = blend(color, float4(results[0].r, results[1].r, results[2].r, (results[0].g + results[1].g + results[2].g) / 3.0));
	}

	color = float4(srgbEncode(color.rgb), color.a);
#if WORKLIST_DEBUG
	color.r += 0.08 * worklistSize;
#endif
	i_target.Store(threadId, color);
}
