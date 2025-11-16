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

RWStructuredBuffer<Primitive> b_primitives;
RWStructuredBuffer<WorklistItem> b_worklists;
RWStructuredBuffer<Atomic<int>> b_worklistCounts;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

func processPrimitive(inout primitive: Primitive)
{
	primitive.color.rgb = srgbDecode(primitive.color.rgb);
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

func enqueuePrimitiveTiles<T>(primitive: Primitive, primitiveIdx: int, aabbFunc: functype(float2, T) -> AABB<float>)
{
	let params = reinterpret<T>(primitive.params);
	let halfVelocity = primitive.velocity / 2.0;
	let head = primitive.position + halfVelocity;
	let tail = primitive.position - halfVelocity * 3.0;
	let headAabb = aabbFunc(head, params);
	let tailAabb = aabbFunc(tail, params);
	let aabb = AABB<float>(
		min(headAabb.top_left, tailAabb.top_left),
		max(headAabb.bottom_right, tailAabb.bottom_right)
	);
	let tileAabb = AABB<int>(
		max(int2(aabb.top_left) / TILE_SIZE, 0),
		min(int2(aabb.bottom_right) / TILE_SIZE + 1, TileBound)
	);
	let tileSpan = tileAabb.bottom_right - tileAabb.top_left;

	for (var y = 0; y < tileSpan.y; y += 1) {
		for (var x = 0; x < tileSpan.x; x += 1) {
			enqueueItem(
				tileAabb.top_left + int2(x, y),
				WorklistItem(primitiveIdx, primitive.group_id)
			);
		}
	}
}

[shader("compute")]
[numthreads(64)]
void computeMain(int threadId: SV_DispatchThreadID, uniform int PrimitiveCount)
{
	if (threadId >= PrimitiveCount) return;

	var primitive = b_primitives[threadId];
	processPrimitive(primitive);
	switch (primitive.type) {
	case Primitive::Type::Rect:
		enqueuePrimitiveTiles(primitive, threadId, rectAabb);
		break;
	case Primitive::Type::Circle:
		enqueuePrimitiveTiles(primitive, threadId, circleAabb);
		break;
	}
	b_primitives[threadId] = primitive;
}
