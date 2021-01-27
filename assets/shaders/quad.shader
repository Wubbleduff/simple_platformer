
@vertex



#version 440 core
layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_uv;

uniform mat4 world;
uniform mat4 mvp;

out vec2 uvs;

void main()
{
    uvs = a_uv;
    gl_Position = mvp * vec4(a_pos.x, a_pos.y, 0.0f, 1.0);
}



@fragment



#version 440 core

uniform sampler2D texture0;
uniform vec4 blend_color;

in vec2 uvs;
in vec4 vertex_position;

out vec4 frag_color;

void main()
{
    frag_color = texture(texture0, uvs) * blend_color;
}


