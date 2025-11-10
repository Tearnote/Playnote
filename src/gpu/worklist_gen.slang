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
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

func enqueueItem(tile: int2, item: WorklistItem)
{
	let tileIdx = tile.y * TileBound.x + tile.x;
	var itemIdx = -1;
	InterlockedAdd(b_worklistCounts[tileIdx], 1, itemIdx);
	let bufIdx = tileIdx * WORKLIST_MAX_SIZE + itemIdx;
	b_worklists[bufIdx] = item;
}

func enqueueCircleTiles(circle: Primitive, primitiveIdx: int)
{
	let params = reinterpret<Primitive::CircleParams>(circle.params);
	let aabb = AABB<float>(
		floor(circle.position - params.radius - 0.5),
		ceil(circle.position + params.radius + 0.5)
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
				WorklistItem(primitiveIdx, circle.group_id)
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
