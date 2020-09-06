
#pragma once

void start_engine();

void stop_engine();

struct PlatformState *get_engine_platform_state();
struct InputState *get_engine_input_state();
struct RendererState *get_engine_renderer_state();

