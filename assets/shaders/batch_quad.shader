
@vertex



#version 440 core
layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_scale;
layout (location = 2) in vec4 a_color;
layout (location = 3) in float a_rotation;

uniform mat4 vp;

out VS_OUT
{
    mat4 mvp;
    vec4 color;
} vs_out;

void main()
{
    float c = cos(a_rotation);
    float s = sin(a_rotation);
    mat4 scaling = mat4(
       a_scale.x, 0.0f, 0.0f, 0.0f,
       0.0f, a_scale.y, 0.0f, 0.0f,
       0.0f, 0.0f, 1.0f, 0.0f,
       0.0f, 0.0f, 0.0f, 1.0f
       );
    mat4 rotating = mat4(
       c, s, 0.0f, 0.0f,
       -s, c, 0.0f, 0.0f,
       0.0f, 0.0f, 1.0f, 0.0f,
       0.0f, 0.0f, 0.0f, 1.0f
       );
    mat4 translating = mat4(
       1.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 1.0f, 0.0f, 0.0f,
       0.0f, 0.0f, 1.0f, 0.0f,
       a_pos.x, a_pos.y, 0.0f, 1.0f
       );
    vs_out.mvp = vp * translating * rotating * scaling;
    vs_out.color = a_color;
    gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0);
};



@geometry



#version 440 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT
{
    mat4 mvp;
    vec4 color;
} gs_in[];

out vec2 uvs;
out vec4 color;

void main() {
    mat4 mvp = gs_in[0].mvp;
    vec4 v_color = gs_in[0].color;

    uvs = vec2(0.0f, 0.0f);
    color = v_color;
    gl_Position = mvp * (gl_in[0].gl_Position + vec4(-0.5f, -0.5f, 0.0f, 0.0f));
    EmitVertex();

    uvs = vec2(1.0f, 0.0f);
    color = v_color;
    gl_Position = mvp * (gl_in[0].gl_Position + vec4( 0.5f, -0.5f, 0.0f, 0.0f));
    EmitVertex();

    uvs = vec2(0.0f, 1.0f);
    color = v_color;
    gl_Position = mvp * (gl_in[0].gl_Position + vec4(-0.5f,  0.5f, 0.0f, 0.0f));
    EmitVertex();

    uvs = vec2(1.0f, 1.0f);
    color = v_color;
    gl_Position = mvp * (gl_in[0].gl_Position + vec4( 0.5f,  0.5f, 0.0f, 0.0f));
    EmitVertex();

    EndPrimitive();
}



@fragment



#version 440 core
uniform sampler2D texture0;

in vec2 uvs;
in vec4 color;

out vec4 frag_color;

void main()
{
    frag_color = texture(texture0, uvs) * color;
//    frag_color = color;
//    frag_color = vec4(uvs.x, uvs.y, 0.0f, 1.0f);
};

