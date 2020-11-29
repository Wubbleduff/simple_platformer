
#pragma once

#include "serialization.h"

struct Levels
{
    struct Level;

    static Level *create_level();

    static void step_level(Level *level, float dt);
    static void draw_level(Level *level);

    static void serialize_level(Level *level, Serialization::Stream *stream);
    static void deserialize_level(Level *level, Serialization::Stream *stream);
};


