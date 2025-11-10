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

StructuredBuffer<Primitive> b_primitives;
RWStructuredBuffer<WorklistItem> b_worklists;
RWStructuredBuffer<int> b_worklistCounts;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + (TILE_SIZE - 1)) % TILE_SIZE;

func enqueueItem(tile: int2, item: WorklistItem)
{
	let tile_idx = tile.y * TileBound.x + tile.x;
	var item_idx = -1;
	InterlockedAdd(b_worklistCounts[tile_idx], 1, item_idx);
	let buf_idx = tile_idx * WORKLIST_MAX_SIZE + item_idx;
	b_worklists[buf_idx] = item;
}

func enqueueCircleTiles(circle: Primitive, primitive_idx: int)
{
	let params = reinterpret<Primitive::CircleParams>(circle.params);
	let aabb = AABB<float>(
		floor(circle.position - params.radius),
		ceil(circle.position + params.radius)
	);
	let tile_aabb = AABB<int>(
		max(int2(aabb.top_left) % TILE_SIZE, 0),
		min(int2(aabb.bottom_right) % TILE_SIZE + 1, TileBound)
	);
	let tile_span = tile_aabb.bottom_right - tile_aabb.top_left;

	for (var y = 0; y < tile_span.y; y += 1) {
		for (var x = 0; x < tile_span.x; x += 1) {
			enqueueItem(
				tile_aabb.top_left + int2(x, y),
				WorklistItem(primitive_idx, circle.group_id)
			);
		}
	}
}

[shader("compute")]
[numthreads(64)]
void computeMain(int threadId: SV_DispatchThreadID, uniform int PrimitiveCount)
{
	if (threadId >= PrimitiveCount) return;

	let primitive = b_primitives[threadId];
	switch (primitive.type) {
	case Primitive::Type::Circle:
		enqueueCircleTiles(primitive, threadId);
		break;
	case Primitive::Type::Rect:
		break; //TODO
	}
}
