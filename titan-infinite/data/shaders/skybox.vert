#version 450 core

layout(set=0, binding=0) uniform TransformUniforms
{
	mat4 skyProjectionMatrix;
};

layout(location=0) in vec3 position;
layout(location=0) out vec3 localPosition;

void main()
{
	localPosition = position.xyz;
	gl_Position   = skyProjectionMatrix * vec4(position, 1.0);
}