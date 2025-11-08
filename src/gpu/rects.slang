/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

import lib;

struct Rect {
    int2 pos;
    int2 size;
    float4 color;
};

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let WindowScale = 0.0;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);

StructuredBuffer<Rect> b_rects;

struct VertexOut {
	float4 position: SV_Position;
	float4 color: COLOR;
};

[shader("vertex")]
VertexOut vertexMain(uint vertexID: SV_VertexID)
{
    let instance = vertexID / 6;
    let vertex = vertexID % 6;

    let rect = b_rects[instance];

    var offset = float2(0.0);
    switch (vertex) {
        case 0:
        offset = {0, 0};
        break;
        case 1:
        offset = {1, 0};
        break;
        case 2:
        offset = {0, 1};
        break;
        case 3:
        offset = {0, 1};
        break;
        case 4:
        offset = {1, 0};
        break;
        case 5:
        offset = {1, 1};
        break;
    }
    var pos = rect.pos + rect.size * offset;
    pos *= WindowScale;
    pos += 0.5; // Center the vertex on the pixel
    pos /= ViewportSize; // from 0.0 to 1.0
    pos = pos * 2.0 - 1.0; // from -1.0 to 1.0 (clip space)

    var out = VertexOut();
    out.position = {pos, 0.0, 1.0};
    out.color = rect.color;
    return out;
}

[shader("fragment")]
float4 fragmentMain(VertexOut in): SV_Target
{
	return float4(srgbEncode(in.color.rgb), in.color.a);
}
