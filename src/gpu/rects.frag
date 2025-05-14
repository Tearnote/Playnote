/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gpu/rects.vert:
Draws solid color rectangles, fragment shader.
*/

#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec4 f_color;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = f_color;
}
