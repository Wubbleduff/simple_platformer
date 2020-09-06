
#pragma once

#include "my_math.h"

struct RendererState;

RendererState *init_renderer();

void clear_frame(RendererState *renderer_state, v4 color);
void render(RendererState *renderer_state);
void swap_frames(RendererState *renderer_state);

void draw_quad(RendererState *renderer_state, v2 position, v2 scale, float rotation, v4 color);

void draw_circle(RendererState *renderer_state, v2 position, float radius, v4 color);

void draw_line(RendererState *renderer_state, v2 a, v2 b, v4 color);

void draw_triangles(RendererState *renderer_state, int num_vertices, v2 *vertices, int num_triples, v2 **triples, v4 color);

v2 ndc_point_to_world(RendererState *renderer_state, v2 ndc);

void set_camera_position(RendererState *renderer_state, v2 position);
void set_camera_width(RendererState *renderer_state, float width);

