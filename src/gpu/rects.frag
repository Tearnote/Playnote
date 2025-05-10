#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec4 f_color;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = f_color;
}
