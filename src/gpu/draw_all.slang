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
Texture2D<float4> i_atlas;

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

func velocityIntegratedRectCoverage(p: float2, v: float2, s: float2) -> float
{
	let t1 = (p - s / 2.0) / v;
	let t2 = (p + s / 2.0) / v;
	let tMinPerAxis = min(t1, t2);
	let tMaxPerAxis = max(t1, t2);
	let tNear = max(tMinPerAxis.x, tMinPerAxis.y);
	let tFar = min(tMaxPerAxis.x, tMaxPerAxis.y);
	if (tNear >= tFar) {
		return 0.0;
	}

	let overlapStart = max(0.0, tNear);
	let overlapEnd = min(1.0, tFar);
	if (overlapStart >= overlapEnd) return 0.0;
	return cosShutterIntegral(overlapStart, overlapEnd);
}

func velocityIntegratedCircleCoverage(p: float2, v: float2, r: float) -> float
{
	let a = dot2(v);
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

func primitiveCoverage(
	primitive: Primitive, at: float2,
	sdfFunc: IFunc<float, float2>,
	velocityIntegratedCoverageFunc: IFunc<float, float2, float2>
) -> float
{
	let scaledVelocity = primitive.velocity * 2;
	let velocityOffset = scaledVelocity / 4;
	let center = at - velocityOffset - primitive.position;
	let velocityLen = length(scaledVelocity);

	if (velocityLen > MaxMotionSupersample) {
		// Analytical motion blur with supersampled AA
		var accum = 0.0;
		for (var i = 0; i < SamplePattern4x.getCount(); i += 1)
			accum += velocityIntegratedCoverageFunc(center + (SamplePattern4x[i] - 0.5), -scaledVelocity);
		return accum / float(SamplePattern4x.getCount());
	} else {
		// Analytical AA with supersampled motion blur
		let sampleCount = clamp(uint(ceil(velocityLen)), 1, MaxMotionSupersample);
		let step = sampleCount == 1? scaledVelocity : scaledVelocity / (sampleCount - 1);
		let shutterStep = 1.0 / float(sampleCount);
		var accum = 0.0;
		for (var i = 0; i < sampleCount; i += 1) {
			let stepCenter = center + step * i;
			let coverage = sdf::aa(sdfFunc(stepCenter));
			let shutter = cosShutterIntegral(shutterStep * i, shutterStep * (i + 1));
			accum += coverage * shutter;
		}
		return accum;
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
	color = float4(srgbDecode(color.rgb), color.a);

	for (int i = 0; i < worklistSize; i += 1) {
		let item = b_worklists[worklistOffset + i];
		let primitive = b_primitives[item.primitive_idx];
		var coverage = 0.0;
		switch (primitive.type) {
		case Primitive::Type::Circle:
			let circleParams = reinterpret<Primitive::CircleParams>(primitive.params);
			coverage = primitiveCoverage(primitive, self_pos,
				(float2 pos) => sdf::circle(pos, circleParams.radius),
				(float2 pos, float2 v) => velocityIntegratedCircleCoverage(pos, v, circleParams.radius)
			);
			break;
		case Primitive::Type::Rect:
			let rectParams = reinterpret<Primitive::RectParams>(primitive.params);
			coverage = primitiveCoverage(primitive, self_pos,
				(float2 pos) => sdf::rect(pos, rectParams.size),
				(float2 pos, float2 v) => velocityIntegratedRectCoverage(pos, v, rectParams.size)
			);
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
