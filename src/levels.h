
#pragma once

#include "serialization.h"
#include "player.h"

struct Levels
{
    struct Level;

    static Level *create_level();

    static void add_avatar(PlayerID id);
    static void remove_avatar(PlayerID id);

    static void step_level(Level *level, float dt);
    static void draw_level(Level *level);

    static void serialize_level(Serialization::Stream *stream, Level *level);
    static void deserialize_level(Serialization::Stream *stream, Level *level);
};


