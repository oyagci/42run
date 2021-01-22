#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 2) in vec2 uv_coords;

out vec2 TexCoords;

void main()
{
	gl_Position = vec4(in_position, 1.0);
	TexCoords = uv_coords;
}
