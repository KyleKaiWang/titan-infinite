#version 450 core

layout(location = 0) in vec3 position;
layout(set = 0, binding = 0) uniform UBO
{
	mat4 mvp;
	vec4 color;
	bool control_point;
};

void main()
{
	gl_PointSize = 5.0f;
	vec3 pos = vec3(position.x, -position.y, position.z);
	gl_Position = mvp * vec4(pos, 1.0);
}