
#pragma once

struct Level;

Level *create_level();

void level_step(Level *level, float dt);

