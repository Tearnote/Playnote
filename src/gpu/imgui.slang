/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

Sampler2D<float4> s_font;

struct VertexIn {
	float2 position;
	float2 uv;
	float4 color;
};

struct VertexOut {
	float4 position: SV_Position;
	float4 color;
	float2 uv;
};

[shader("vertex")]
VertexOut vertexMain(VertexIn in, uniform float2 Scale, uniform float2 Translate)
{
	var out = VertexOut();
	out.position = {in.position * Scale + Translate, 0.0, 1.0};
	out.color = in.color;
	out.uv = in.uv;
	return out;
}

[shader("fragment")]
float4 fragmentMain(VertexOut in): SV_Target
{
	return in.color * s_font.Sample(in.uv);
}
