
#pragma once

#include "game.h"
#include "serialization.h"

struct Levels
{
    struct Level;

    static Level *create_level();

    static void add_avatar(Level *level, Game::PlayerID id);
    static void remove_avatar(Level *level, Game::PlayerID id);

    static void step_level(Level *level, float dt);
    static void draw_level(Level *level);

    static void serialize_level(Serialization::Stream *stream, Level *level);
    static void deserialize_level(Serialization::Stream *stream, Level *level);
};


