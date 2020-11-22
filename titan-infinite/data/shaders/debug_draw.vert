#version 450 core

layout(location = 0) in vec3 position;
layout(set = 0, binding = 0) uniform UBO
{
	mat4 mvp;
	vec4 color;
};

void main()
{
	gl_Position = mvp * vec4(position, 1.0);
}