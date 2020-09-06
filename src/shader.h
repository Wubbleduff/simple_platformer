
#pragma once

#include "GL/glew.h"
#include "GL/wglew.h"
#include "my_math.h"

struct Shader;

Shader *make_shader(const char *shader_path);

void use_shader(Shader *shader);

void set_uniform(Shader *shader, const char *name, v4 value);
void set_uniform(Shader *shader, const char *name, mat4 value);

