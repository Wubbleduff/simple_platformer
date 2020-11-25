
@vertex



#version 440 core
layout (location = 0) in vec2 a_pos;

uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(a_pos.x, a_pos.y, 0.0f, 1.0);
}



@fragment



#version 440 core

uniform vec4 blend_color;

out vec4 frag_color;

void main()
{
    frag_color = blend_color;
}


