#version 450 core

layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform UBO 
{
	mat4 mvp;
	vec4 color;
};
void main()
{
	FragColor = color;
}