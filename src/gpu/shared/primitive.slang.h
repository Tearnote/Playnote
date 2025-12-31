/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once

struct Primitive {
	struct RectParams {
		float2 size;
		int _pad0[2];
		int _pad1[4];
	};
	struct CircleParams {
		float radius;
		int _pad0[3];
		int _pad1[4];
	};
	struct GlyphParams {
		AABB<float> atlas_bounds;
		float size;
		int page;
		int _pad0[2];
	};

	enum class Type: int {
		Rect,
		Circle,
		Glyph,
	};
	Type type;
	int group_id;
	float2 position;
	float4 color;
	float4 outline_color;
	float4 glow_color;
	float rotation;
	float outline_width;
	float glow_width;
	int _pad0[1];
#ifndef __cplusplus
	int params[8]; // Space containing one of the union members below, as per the type
#else
	union {
		RectParams rect_params;
		CircleParams circle_params;
		GlyphParams glyph_params;
	};
#endif
};

#ifndef __cplusplus
func readPrimitive(buf: RWByteAddressBuffer, idx: int) -> Primitive
{ return buf.LoadByteOffset<Primitive>(idx * sizeof(Primitive)); }

func writePrimitive(buf: RWByteAddressBuffer, idx: int, prim: Primitive)
{ buf.StoreByteOffset(idx * sizeof(Primitive), prim); }

func readPrimitiveParams(buf: RWByteAddressBuffer, idx: int) -> int[8]
{ return buf.LoadByteOffset<int[8]>((idx + 1) * sizeof(Primitive) - sizeof(int[8])); }

func writePrimitiveParams<T>(buf: RWByteAddressBuffer, idx: int, p: T)
{ buf.StoreByteOffset((idx + 1) * sizeof(Primitive) - sizeof(int[8]), p); }
#endif
