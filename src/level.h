
#pragma once

#include "serialization.h"

struct Level;

Level *create_level();

void level_step(Level *level, float dt);

void serialize_level(Level *level, Serialization::Stream *stream);

