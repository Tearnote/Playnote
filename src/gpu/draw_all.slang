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

RWByteAddressBuffer b_primitives;
StructuredBuffer<WorklistItem> b_worklists;
StructuredBuffer<int> b_worklistSizes;
Sampler2D<float4> s_staticAtlas;
Sampler2D<float4> s_dynamicAtlas;
RWTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let SubpixelRenderingRaw = 0u;
[SpecializationConstant] let AtlasDistanceRange = 0.0;
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

func sampleGlyphDistance(p: float2, params: Primitive::GlyphParams) -> float2
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
	var s: float4;
	if (params.page == 0)
		s = s_staticAtlas.SampleLevel(uv, 0);
	else
		s = s_dynamicAtlas.SampleLevel(uv, 0);
	let sd = msdfMedian(s.r, s.g, s.b);
	let sds = float2(sd, s.a);
	let dists = (0.5 - sds) * AtlasDistanceRange;
	let distOutside = length(texel - clampedTexel);
	return (dists + distOutside) * params.size;
}

func distanceToPrimitive(primitive: Primitive, params: int[8], at: float2) -> float2
{
	let center = at - primitive.position;
	var dist: float;
	switch (primitive.type) {
	case Primitive::Type::Circle:
		let circleParams = reinterpret<Primitive::CircleParams>(params);
		dist = sdf::circle(center, circleParams.radius);
		return float2(dist, dist);
	case Primitive::Type::Rect:
		let rectParams = reinterpret<Primitive::RectParams>(params);
		dist = sdf::rect(center, rectParams.size);
		return float2(dist, dist);
	case Primitive::Type::Glyph:
		let glyphParams = reinterpret<Primitive::GlyphParams>(params);
		return sampleGlyphDistance(center, glyphParams);
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
			let distance = distanceToPrimitive(primitive, params, selfPos);

			let outlineCoverage = sdf::aa(distance.x - primitive.outline_width / 2.0) * sdf::aa(-(distance.x + primitive.outline_width / 2.0));
			let fillCoverage = sdf::aa(distance.x);
			var glowPower = saturate(1.0 - abs(distance.y) / primitive.glow_width);
			glowPower = glowPower * glowPower * glowPower;

			color = blend(color, primitive.glow_color * glowPower);
			color = blend(color, primitive.color * fillCoverage);
			color = blend(color, primitive.outline_color * outlineCoverage);
		}
	} else {
		var results: float2[3] = {};
		for (int j = 0; j < 3; j += 1) {
			for (int i = 0; i < worklistSize; i += 1) {
				let item = worklist[i];
				let primitive = readPrimitive(b_primitives, item.primitive_idx);
				let params = readPrimitiveParams(b_primitives, item.primitive_idx);
				let distance = distanceToPrimitive(primitive, params, selfPos + (SubpixelLayoutRGB[j] - float2(0.5, 0.5)));

				let outlineCoverage = sdf::aa(distance.x - primitive.outline_width / 2.0) * sdf::aa(-(distance.x + primitive.outline_width / 2.0));
				let fillCoverage = sdf::aa(distance.x);
				var glowPower = saturate(1.0 - abs(distance.y) / primitive.glow_width);
				glowPower = glowPower * glowPower * glowPower;

				let fillColorComponent = float2(primitive.color[j], primitive.color.a);
				let outlineColorComponent = float2(primitive.outline_color[j], primitive.outline_color.a);
				let glowColorComponent = float2(primitive.glow_color[j], primitive.glow_color.a);

				results[j] = blend(results[j], glowColorComponent * glowPower);
				results[j] = blend(results[j], fillColorComponent * fillCoverage);
				results[j] = blend(results[j], outlineColorComponent * outlineCoverage);
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
