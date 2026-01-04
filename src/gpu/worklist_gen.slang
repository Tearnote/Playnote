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
#include "shared/transform.slang.h"
#include "shared/worklist.slang.h"

RWByteAddressBuffer b_primitives;
RWStructuredBuffer<WorklistItem> b_worklists;
RWStructuredBuffer<Atomic<int>> b_worklistCounts;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let AtlasPixelsPerEm = 0.0;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

func transformPoint(Transform t, float2 p) -> float2
{ return p * t.scale + t.offset; }

func transformScalar(Transform t, float s) -> float
{ return s * t.scale; }

func transformScalar(Transform t, float2 s) -> float2
{ return s * t.scale; }

func enqueueItem(tile: int2, item: WorklistItem)
{
	let tileIdx = tile.y * TileBound.x + tile.x;
	var itemIdx = b_worklistCounts[tileIdx].increment();
	let bufIdx = tileIdx * WORKLIST_MAX_SIZE + itemIdx;
	b_worklists[bufIdx] = item;
}

func pieAABB(float2 position, Primitive::PieParams params) -> AABB<float>
{
	return AABB<float>(
		floor(position - params.radius - 0.5),
		ceil(position + params.radius + 0.5)
	);
}

func rectAABB(float2 position, Primitive::RectParams params) -> AABB<float>
{
	return AABB<float>(
		floor(position - params.size / 2.0 - 0.5),
		ceil(position + params.size / 2.0 + 0.5)
	);
}

func capsuleAABB(float2 position, Primitive::CapsuleParams params) -> AABB<float>
{
	return AABB<float>(
		floor(position - params.width / 2.0 - params.radius - 0.5),
		ceil(position + params.width / 2.0 + params.radius + 0.5)
	);
}

func glyphAABB(float2 position, Primitive::GlyphParams params) -> AABB<float>
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
	uniform Transform transform,
	uniform int primitiveCount
) {
	if (threadId >= primitiveCount) return;
	var primitive = readPrimitive(b_primitives, threadId);
	var params = readPrimitiveParams(b_primitives, threadId);

	// Initial processing
	primitive.position = transformPoint(transform, primitive.position);
	primitive.scissor.top_left = transformPoint(transform, primitive.scissor.top_left);
	primitive.scissor.bottom_right = transformPoint(transform, primitive.scissor.bottom_right);
	primitive.outline_width = transformScalar(transform, primitive.outline_width);
	primitive.glow_width = transformScalar(transform, primitive.glow_width);

	switch (primitive.type) {
	case Primitive::Type::Pie:
		var pieParams = reinterpret<Primitive::PieParams>(params);
		pieParams.radius = transformScalar(transform, pieParams.radius);
		params = reinterpret<int[8]>(pieParams);
		break;
	case Primitive::Type::Rect:
		var rectParams = reinterpret<Primitive::RectParams>(params);
		rectParams.size = transformScalar(transform, rectParams.size);
		params = reinterpret<int[8]>(rectParams);
		break;
	case Primitive::Type::Capsule:
		var capsuleParams = reinterpret<Primitive::CapsuleParams>(params);
		capsuleParams.width = transformScalar(transform, capsuleParams.width);
		capsuleParams.radius = transformScalar(transform, capsuleParams.radius);
		params = reinterpret<int[8]>(capsuleParams);
		break;
	case Primitive::Type::Glyph:
		var glyphParams = reinterpret<Primitive::GlyphParams>(params);
		glyphParams.size = transformScalar(transform, glyphParams.size / AtlasPixelsPerEm);
		params = reinterpret<int[8]>(glyphParams);
		break;
	}

	// Calculate AABB
	var aabb = AABB<float>();
	switch (primitive.type) {
	case Primitive::Type::Pie:
		aabb = pieAABB(primitive.position, reinterpret<Primitive::PieParams>(params));
		break;
	case Primitive::Type::Rect:
		aabb = rectAABB(primitive.position, reinterpret<Primitive::RectParams>(params));
		break;
	case Primitive::Type::Capsule:
		aabb = capsuleAABB(primitive.position, reinterpret<Primitive::CapsuleParams>(params));
		break;
	case Primitive::Type::Glyph:
		aabb = glyphAABB(primitive.position, reinterpret<Primitive::GlyphParams>(params));
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
	top_left = rotateAround(top_left, primitive.position, -primitive.rotation);
	bottom_right = rotateAround(bottom_right, primitive.position, -primitive.rotation);
	top_right = rotateAround(top_right, primitive.position, -primitive.rotation);
	bottom_left = rotateAround(bottom_left, primitive.position, -primitive.rotation);
	aabb.top_left = min(top_left, min(bottom_right, min(top_right, bottom_left)));
	aabb.bottom_right = max(top_left, max(bottom_right, max(top_right, bottom_left)));

	// Apply scissor
	aabb.top_left = max(aabb.top_left, primitive.scissor.top_left);
	aabb.bottom_right = min(aabb.bottom_right, primitive.scissor.bottom_right);

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
