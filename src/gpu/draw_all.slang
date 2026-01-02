/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

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
Sampler2D<float4> s_staticAtlas;
Sampler2D<float4> s_dynamicAtlas;
WTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let SubpixelLayoutRaw = 0u;
[SpecializationConstant] let AtlasDistanceRange = 0.0;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

// Matching the enum in lib/os.hpp
enum class SubpixelLayout {
	None,
	HorizontalRGB,
	HorizontalBGR,
	VerticalRGB,
	VerticalBGR,
};
static let SubpixelLayoutType = SubpixelLayout(SubpixelLayoutRaw);

static let SubpixelsHorizontalRGB: float2[] = {
	{0.166, 0.5},
	{0.500, 0.5},
	{0.833, 0.5},
};
static let SubpixelsHorizontalBGR: float2[] = {
	{0.833, 0.5},
	{0.500, 0.5},
	{0.166, 0.5},
};
static let SubpixelsVerticalRGB: float2[] = {
	{0.5, 0.166},
	{0.5, 0.500},
	{0.5, 0.833},
};
static let SubpixelsVerticalBGR: float2[] = {
	{0.5, 0.833},
	{0.5, 0.500},
	{0.5, 0.166},
};

static let MaxDistance = 1e5;

// Worklist cache for the current tile.
groupshared var worklist: WorklistItem[WORKLIST_MAX_SIZE];

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
	let sd = median(s.r, s.g, s.b);
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
	case Primitive::Type::Capsule:
		let capsuleParams = reinterpret<Primitive::CapsuleParams>(params);
		dist = sdf::capsule(center, capsuleParams.width, capsuleParams.radius);
		return float2(dist, dist);
	case Primitive::Type::Glyph:
		let glyphParams = reinterpret<Primitive::GlyphParams>(params);
		return sampleGlyphDistance(center, glyphParams);
	default: return 0.0;
	}
}

func shadePrimitiveDist<int N>(
	distance: float2, source: vector<float, N>, at: float2, extCoverage: float,
	primitive: Primitive, params: int[8],
	extractColor: IFunc<vector<float, N>, float4>
) -> vector<float, N>
{
	let outlineCoverage = sdf::aa(distance.x - primitive.outline_width / 2.0) * sdf::aa(-(distance.x + primitive.outline_width / 2.0));
	let fillCoverage = sdf::aa(distance.x);
	let distRatio = saturate(abs(distance.y) / primitive.glow_width);
	let glowPower = saturate(exp(-4.0 * distRatio * distRatio) * (1.0 - distRatio));

	// Composite the primitive layers
	var result = source;
	result = blend(result, extractColor(primitive.glow_color) * glowPower * extCoverage);
	result = blend(result, extractColor(primitive.color) * fillCoverage * extCoverage);
	result = blend(result, extractColor(primitive.outline_color) * outlineCoverage * extCoverage);
	return result;
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
	var color = float4(0.0, 0.0, 0.0, 1.0);

	if (SubpixelLayoutType == SubpixelLayout::None) {
		// Per-pixel coverage
		var distance = float2(MaxDistance, MaxDistance);
		for (int i = 0; i < worklistSize; i += 1) {
			let workItem = worklist[i];
			let primitive = readPrimitive(b_primitives, workItem.primitive_idx);
			let params = readPrimitiveParams(b_primitives, workItem.primitive_idx);
			let next_group = i == worklistSize - 1 ? -1 : worklist[i + 1].group_id;

			// Scissor clip
			let scissorMidpoint = (primitive.scissor.top_left + primitive.scissor.bottom_right) / 2.0;
			let scissorCoverage = sdf::aa(sdf::rect(selfPos - scissorMidpoint, primitive.scissor.bottom_right - primitive.scissor.top_left));
			if (scissorCoverage <= 0.0) continue;

			// Rotate the primitive counterclockwise by rotating the sample point clockwise
			let rotatedPos = rotateAround(selfPos, primitive.position, primitive.rotation);
			distance = min(distance, distanceToPrimitive(primitive, params, rotatedPos));

			if (workItem.group_id != next_group) {
				color = shadePrimitiveDist(distance, color, rotatedPos, scissorCoverage, primitive, params,
					(float4 c) => c
				);
				distance = float2(MaxDistance, MaxDistance);
			}
		}
	} else {
		// Per-subpixel coverage
		var results: float2[3] = {};
		var distance = float2[3](
			MaxDistance, MaxDistance,
			MaxDistance, MaxDistance,
			MaxDistance, MaxDistance,
		);
		for (int i = 0; i < worklistSize; i += 1) {
			let workItem = worklist[i];
			let primitive = readPrimitive(b_primitives, workItem.primitive_idx);
			let params = readPrimitiveParams(b_primitives, workItem.primitive_idx);
			let next_group = i == worklistSize - 1 ? -1 : worklist[i + 1].group_id;
			for (int j = 0; j < 3; j += 1) {
				var subpixOffset: float2;
				switch (SubpixelLayoutType) {
					case SubpixelLayout::HorizontalRGB: subpixOffset = SubpixelsHorizontalRGB[j]; break;
					case SubpixelLayout::HorizontalBGR: subpixOffset = SubpixelsHorizontalBGR[j]; break;
					case SubpixelLayout::VerticalRGB:   subpixOffset = SubpixelsVerticalRGB[j]; break;
					default:                            subpixOffset = SubpixelsVerticalBGR[j]; break;
				}
				let subpixPos = selfPos + (subpixOffset - float2(0.5, 0.5));

				// Scissor clip
				let scissorMidpoint = (primitive.scissor.top_left + primitive.scissor.bottom_right) / 2.0;
				let scissorCoverage = sdf::aa(sdf::rect(subpixPos - scissorMidpoint, primitive.scissor.bottom_right - primitive.scissor.top_left));
				if (scissorCoverage <= 0.0) continue;

				// Rotate the primitive counterclockwise by rotating the sample point clockwise
				let rotatedPos = rotateAround(subpixPos, primitive.position, primitive.rotation);
				distance[j] = min(distance[j], distanceToPrimitive(primitive, params, rotatedPos));

				if (workItem.group_id != next_group) {
					results[j] = shadePrimitiveDist(distance[j], results[j], rotatedPos, scissorCoverage, primitive, params,
						(float4 c) => float2(c[j], c.a)
					);
					distance[j] = float2(MaxDistance, MaxDistance);
				}
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
