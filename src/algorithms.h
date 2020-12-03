
#pragma once

#include "game_math.h"

void my_sort(void *base, size_t num, size_t element_bytes, int (*cmp)(const void *a, const void *b));

void find_convex_hull(int num_vertices, GameMath::v2 *vertices, int *num_hull_lines, GameMath::v2 **hull);
