
#pragma once

#include "my_math.h"

struct InputState;

InputState *init_input();

bool key_toggled_down(InputState *input_state, int button);
bool key_state(InputState *input_state, int button);

bool mouse_toggled_down(InputState *input_state, int button);
bool mouse_state(InputState *input_state, int button);
v2 mouse_world_position(InputState *input_state);

void read_input(InputState *input_state);
void record_key_event(InputState *input_state, int vk_code, bool state);
void record_mouse_event(InputState *input_state, int vk_code, bool state);

