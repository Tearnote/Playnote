#version 460
#pragma shader_stage(vertex)

struct Rect {
	ivec2 pos;
	ivec2 size;
	vec4 color;
};

layout(binding = 0, std430) restrict readonly buffer Rects {
	Rect b_rects[];
};

layout (constant_id = 0) const uint ViewportWidth = 0;
layout (constant_id = 1) const uint ViewportHeight = 0;
const uvec2 ViewportSize = uvec2(ViewportWidth, ViewportHeight);

layout(location = 0) out vec4 f_color;


void main() {
	int instance = gl_VertexIndex / 6;
	int vertex = gl_VertexIndex % 6;

	Rect rect = b_rects[instance];

	vec2 offset;
	switch (vertex) {
		case 0: offset = vec2(0, 0); break;
		case 1: offset = vec2(1, 0); break;
		case 2: offset = vec2(0, 1); break;
		case 3: offset = vec2(0, 1); break;
		case 4: offset = vec2(1, 0); break;
		case 5: offset = vec2(1, 1); break;
	}
	vec2 pos = rect.pos + rect.size * offset;

	gl_Position = vec4(pos / vec2(ViewportSize) * 2.0 - 1.0, 0.0, 1.0);
	f_color = rect.color;
}
