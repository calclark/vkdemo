#version 460

layout(binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 projection;
}
ubo;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 vert_color;
layout(location = 2) in vec2 vert_tex_coords;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 frag_tex_coords;

void main()
{
	gl_Position =
			ubo.projection * ubo.view * ubo.model * vec4(position, 1.0);
	frag_color = vert_color;
	frag_tex_coords = vert_tex_coords;
}
