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

RWByteAddressBuffer b_primitives;
RWStructuredBuffer<WorklistItem> b_worklists;
RWStructuredBuffer<Atomic<int>> b_worklistCounts;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let AtlasPixelsPerEm = 0.0;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

func unpackTransform(float4 packed) -> float3x3
{
	let offset = packed.xy;
	let scale = packed.zw;
	return {
		{scale.x,     0.0, offset.x},
		{    0.0, scale.y, offset.y},
		{    0.0,     0.0,      1.0}
	};
}

func enqueueItem(tile: int2, item: WorklistItem)
{
	let tileIdx = tile.y * TileBound.x + tile.x;
	var itemIdx = b_worklistCounts[tileIdx].increment();
	let bufIdx = tileIdx * WORKLIST_MAX_SIZE + itemIdx;
	b_worklists[bufIdx] = item;
}

func rectAabb(float2 position, Primitive::RectParams params) -> AABB<float>
{
	return AABB<float>(
		floor(position - params.size / 2.0 - 0.5),
		ceil(position + params.size / 2.0 + 0.5)
	);
}

func circleAabb(float2 position, Primitive::CircleParams params) -> AABB<float>
{
	return AABB<float>(
		floor(position - params.radius - 0.5),
		ceil(position + params.radius + 0.5)
	);
}

func glyphAabb(float2 position, Primitive::GlyphParams params) -> AABB<float>
{
	return AABB<float>(
		floor(position),
		ceil(position + (params.atlas_bounds.bottom_right - params.atlas_bounds.top_left) * params.size)
	);
}

[shader("compute")]
[numthreads(64)]
void computeMain(
	int threadId: SV_DispatchThreadID,
	uniform float4 Transform,
	uniform int PrimitiveCount
) {
	if (threadId >= PrimitiveCount) return;
	var primitive = readPrimitive(b_primitives, threadId);
	var params = readPrimitiveParams(b_primitives, threadId);

	// Initial processing
	let transform = unpackTransform(Transform);
	primitive.position = mul(transform, float3(primitive.position, 1.0)).xy;
	primitive.rotation = radians(primitive.rotation);
	primitive.outline_width = mul(transform, float3(primitive.outline_width, 0.0, 0.0)).x;
	primitive.glow_width = mul(transform, float3(primitive.glow_width, 0.0, 0.0)).x;
	primitive.color.rgb = srgbDecode(primitive.color.rgb);
	primitive.outline_color.rgb = srgbDecode(primitive.outline_color.rgb);
	primitive.glow_color.rgb = srgbDecode(primitive.glow_color.rgb);

	switch (primitive.type) {
	case Primitive::Type::Rect:
		var rectParams = reinterpret<Primitive::RectParams>(params);
		rectParams.size = mul(transform, float3(rectParams.size, 0.0)).xy;
		params = reinterpret<int[8]>(rectParams);
		break;
	case Primitive::Type::Circle:
		var circleParams = reinterpret<Primitive::CircleParams>(params);
		circleParams.radius = mul(transform, float3(circleParams.radius, 0.0, 0.0)).x;
		params = reinterpret<int[8]>(circleParams);
		break;
	case Primitive::Type::Glyph:
		var glyphParams = reinterpret<Primitive::GlyphParams>(params);
		glyphParams.size = mul(transform, float3(glyphParams.size / AtlasPixelsPerEm, 0.0, 0.0)).x;
		params = reinterpret<int[8]>(glyphParams);
		break;
	}

	// Calculate AABB
	var aabb = AABB<float>();
	switch (primitive.type) {
	case Primitive::Type::Rect:
		aabb = rectAabb(primitive.position, reinterpret<Primitive::RectParams>(params));
		break;
	case Primitive::Type::Circle:
		aabb = circleAabb(primitive.position, reinterpret<Primitive::CircleParams>(params));
		break;
	case Primitive::Type::Glyph:
		aabb = glyphAabb(primitive.position, reinterpret<Primitive::GlyphParams>(params));
		break;
	}
	let edgeExtend = max(primitive.outline_width / 2.0, primitive.glow_width);
	aabb.top_left -= edgeExtend;
	aabb.bottom_right += edgeExtend;

	// Rotate the AABB
	var top_left = aabb.top_left;
	var bottom_right = aabb.bottom_right;
	var top_right = float2(bottom_right.x, top_left.y);
	var bottom_left = float2(top_left.x, bottom_right.y);
	top_left = rotateAround(top_left, primitive.position, primitive.rotation);
	bottom_right = rotateAround(bottom_right, primitive.position, primitive.rotation);
	top_right = rotateAround(top_right, primitive.position, primitive.rotation);
	bottom_left = rotateAround(bottom_left, primitive.position, primitive.rotation);
	aabb.top_left = min(top_left, min(bottom_right, min(top_right, bottom_left)));
	aabb.bottom_right = max(top_left, max(bottom_right, max(top_right, bottom_left)));

	// Compute tile coverage
	let tileAabb = AABB<int>(
		max(int2(aabb.top_left) / TILE_SIZE, 0),
		min(int2(aabb.bottom_right) / TILE_SIZE + 1, TileBound)
	);
	let tileSpan = tileAabb.bottom_right - tileAabb.top_left;

	// Write out tiles
	for (var y = 0; y < tileSpan.y; y += 1) {
		for (var x = 0; x < tileSpan.x; x += 1) {
			enqueueItem(
				tileAabb.top_left + int2(x, y),
				WorklistItem(threadId, primitive.group_id)
			);
		}
	}

	// Write out processed primitive
	writePrimitive(b_primitives, threadId, primitive);
	writePrimitiveParams(b_primitives, threadId, params);
}
