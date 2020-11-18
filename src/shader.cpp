
#include "shader.h"
#include "platform.h"
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>



struct Shader
{
    GLuint program;
};




/*
static void check_gl_errors(const char *desc)
{
    GLint error = glGetError();
    if(error)
    {
        log_error("Error %i: %s\n", error, desc);
        assert(false);
    }
}
*/

static bool is_newline(char *c)
{
    if(c[0] == '\n' || c[0] == '\r' ||
      (c[0] == '\r' && c[1] == '\n'))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void read_shader_file(const char *path, char **memory, char **vert_source, char **geom_source, char **frag_source)
{
    *memory = nullptr;
    *vert_source = nullptr;
    *geom_source = nullptr;
    *frag_source = nullptr;

    char *file = read_file_as_string(path);
    if(file == nullptr) return;

    *memory = file;

    int current_tag_length = 0;
    char *current_tag = nullptr;
    char *current_source_start = nullptr;

    char *character = file;
    while(*character != '\0')
    {
        if(*character == '@')
        {
            // Finish reading current shader source
            if(current_tag == nullptr)
            {
            }
            else if(strncmp("vertex", current_tag, current_tag_length) == 0)
            {
                *vert_source = current_source_start;
            }
            else if(strncmp("geometry", current_tag, current_tag_length) == 0)
            {
                *geom_source = current_source_start;
            }
            else if(strncmp("fragment", current_tag, current_tag_length) == 0)
            {
                *frag_source = current_source_start;
            }


            // Null terminate previous shader string
            *character = '\0';

            // Read tag
            character++;
            char *tag = character;

            // Move past tag
            while(!is_newline(character))
            {
                character++;
            }
            char *one_past_end_tag = character;
            while(is_newline(character)) character++;

            current_tag_length = one_past_end_tag - tag;
            current_tag = tag;
            current_source_start = character;
        }
        else
        {
            character++;
        }
    }

    // Finish reading current shader source
    if(current_tag == nullptr)
    {
    }
    else if(strncmp("vertex", current_tag, current_tag_length) == 0)
    {
        *vert_source = current_source_start;
    }
    else if(strncmp("geometry", current_tag, current_tag_length) == 0)
    {
        *geom_source = current_source_start;
    }
    else if(strncmp("fragment", current_tag, current_tag_length) == 0)
    {
        *frag_source = current_source_start;
    }
}



Shader *make_shader(const char *shader_path)
{
    Shader *result = (Shader *)my_allocate(sizeof(Shader));

    int  success;
    char info_log[512];

    char *memory = nullptr;
    char *vert_source = nullptr;
    char *geom_source = nullptr;
    char *frag_source = nullptr;
    read_shader_file(shader_path, &memory, &vert_source, &geom_source, &frag_source);

    unsigned int vert_shader;
    vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_source, NULL);
    glCompileShader(vert_shader);
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(vert_shader, 512, NULL, info_log);
        log_error("VERTEX SHADER ERROR : %s\n%s\n", shader_path, info_log);
    }

    unsigned int frag_shader;
    frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_source, NULL);
    glCompileShader(frag_shader);
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(frag_shader, 512, NULL, info_log);
        log_error("VERTEX SHADER ERROR : %s\n%s\n", shader_path, info_log);
    }


    unsigned int geom_shader;
    if(geom_source != nullptr)
    {
        geom_shader = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(geom_shader, 1, &geom_source, NULL);
        glCompileShader(geom_shader);
        glGetShaderiv(geom_shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(geom_shader, 512, NULL, info_log);
            log_error("VERTEX SHADER ERROR : %s\n%s\n", shader_path, info_log);
        }
    }

    check_gl_errors("compiling shaders");

    GLuint program = glCreateProgram();
    check_gl_errors("making program");

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    if(geom_source != nullptr) glAttachShader(program, geom_shader);
    glLinkProgram(program);
    check_gl_errors("linking program");

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success)
    {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        log_error("SHADER LINKING ERROR : %s\n%s\n", shader_path, info_log);
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader); 
    check_gl_errors("deleting shaders");

    result->program = program;

    return result;
}

void use_shader(Shader *shader)
{
    glUseProgram(shader->program);
    check_gl_errors("use program");
}

void set_uniform(Shader *shader, const char *name, v4 value)
{
    GLint loc = glGetUniformLocation(shader->program, name);
    glUniform4f(loc, value.x, value.y, value.z, value.w);
    //if(loc == -1) log_error("Could not find uniform \"%s\"", name);
}

void set_uniform(Shader *shader, const char *name, mat4 value)
{
    GLint loc = glGetUniformLocation(shader->program, name);
    glUniformMatrix4fv(loc, 1, true, &(value[0][0]));
    //if(loc == -1) log_error("Could not find uniform \"%s\"", name);
}

