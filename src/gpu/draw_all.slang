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

StructuredBuffer<Primitive> b_primitives;
StructuredBuffer<WorklistItem> b_worklists;
StructuredBuffer<int> b_worklistSizes;
RWTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

static let MaxMotionSupersample = 4u;
static let SamplePattern4x: float2[4] = {
	{0.375, 0.125},
	{0.875, 0.375},
	{0.125, 0.625},
	{0.625, 0.875}
};

func supersample<let N: int, let M: int>(position: float2, pattern: float2[M], sampleFunc: IFunc<vector<float, N>, float2>) -> vector<float, N>
{
	var result = vector<float, N>(0.0);
	for (var i = 0; i < M; i += 1)
		result += sampleFunc(position + (pattern[i] - 0.5));
	return result / float(M);
}

func motionBlurredCircleCoverage(p: float2, v: float2, r: float) -> float
{
	let a = dot2(v);
	if (a < 0.001) return sdf::circle(p, r) <= 0.0? 1.0 : 0.0;
	let b = -2.0 * dot(p, v);
	let c = dot2(p) - r * r;

	let delta = b * b - 4.0 * a * c;
	if (delta < 0.0) return 0.0;

	let sqrtDelta = sqrt(delta);
	let t1 = (-b - sqrtDelta) / (2.0 * a);
	let t2 = (-b + sqrtDelta) / (2.0 * a);

	let overlapStart = max(0.0, t1);
	let overlapEnd = min(1.0, t2);
	return cosShutterIntegral(overlapStart, overlapEnd);
}

func circleCoverage(circle: Primitive, at: float2) -> float
{
	let params = reinterpret<Primitive::CircleParams>(circle.params);
	let scaledVelocity = circle.velocity * 2;
	let velocityOffset = scaledVelocity / 4;
	let center = at - velocityOffset - circle.position;
	let velocityLen = length(scaledVelocity);

	if (velocityLen > MaxMotionSupersample) {
		// Analytical motion blur with supersampled AA
		return supersample(center, SamplePattern4x, (float2 position) =>
			vector<float, 1>(motionBlurredCircleCoverage(position, -scaledVelocity, params.radius))
		);
	} else {
		// Analytical AA with supersampled motion blur
		let sampleCount = clamp(uint(ceil(velocityLen)), 1, MaxMotionSupersample);
		let step = sampleCount == 1? -scaledVelocity : -scaledVelocity / (sampleCount - 1);
		let shutterStep = 1.0 / sampleCount;
		var accumAlpha = 0.0;
		for (var i = 0u; i < sampleCount; i += 1) {
			let stepCenter = center - step * i;
			let alpha = sdf::aa(sdf::circle(stepCenter, params.radius));
			let shutter = cosShutterIntegral(shutterStep * i, shutterStep * (i + 1));
			accumAlpha += alpha * shutter;
		}
		return accumAlpha;
	}
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
#if WORKLIST_DEBUG
	color += float4(0.1 * worklistSize, 0.0, 0.0, 1.0);
#endif
	color = float4(srgbDecode(color.rgb), color.a);

	for (int i = 0; i < worklistSize; i += 1) {
		let item = b_worklists[worklistOffset + i];
		let primitive = b_primitives[item.primitive_idx];
		switch (primitive.type) {
		case Primitive::Type::Circle:
			let coverage = circleCoverage(primitive, self_pos);
			color = blend(color, float4(primitive.color.rgb, primitive.color.a * coverage));
			break;
		case Primitive::Type::Rect:
			break; //TODO
		}
	}

	color = float4(srgbEncode(color.rgb), color.a);
	i_target.Store(threadId, color);
}
