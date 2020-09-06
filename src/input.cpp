
#include "input.h"
#include "memory.h"
#include "engine.h"
#include "platform.h"
#include "renderer.h"

#include <windows.h>
#include <assert.h>

static const int MAX_KEYS = 256;
static const int MAX_MOUSE_KEYS = 8;
struct InputState
{
  bool current_key_states[MAX_KEYS];
  bool previous_key_states[MAX_KEYS];
  bool event_key_states[MAX_KEYS]; // For recording windows key press events

  bool current_mouse_states[MAX_MOUSE_KEYS];
  bool previous_mouse_states[MAX_MOUSE_KEYS];
  bool event_mouse_states[MAX_MOUSE_KEYS]; // For recording windows key press events
};





// For recording windows key press events
void record_key_event(InputState *input_state, int vk_code, bool state)
{
  input_state->event_key_states[vk_code] = state;
}

// For recording windows key press events
void record_mouse_event(InputState *input_state, int vk_code, bool state)
{
  input_state->event_mouse_states[vk_code] = state;
}


void read_input(InputState *input_state)
{
    // Read keyboard input
    memcpy(input_state->previous_key_states, input_state->current_key_states, sizeof(bool) * MAX_KEYS);
    memcpy(input_state->current_key_states, input_state->event_key_states, sizeof(bool) * MAX_KEYS);

    // Read mouse input
    memcpy(input_state->previous_mouse_states, input_state->current_mouse_states, sizeof(bool) * MAX_MOUSE_KEYS);
    memcpy(input_state->current_mouse_states, input_state->event_mouse_states, sizeof(bool) * MAX_MOUSE_KEYS);
}


bool key_toggled_down(InputState *input_state, int key)
{
  assert(key < MAX_KEYS);

  return (input_state->previous_key_states[key] == false && input_state->current_key_states[key] == true) ? true : false;
}

bool key_state(InputState *input_state, int key)
{
  assert(key < MAX_KEYS);

  return input_state->current_key_states[key];
}


bool mouse_toggled_down(InputState *input_state, int key)
{
  assert(key < MAX_MOUSE_KEYS);

  return (input_state->previous_mouse_states[key] == false && input_state->current_mouse_states[key] == true) ? true : false;
}

bool mouse_state(InputState *input_state, int key)
{
  assert(key < MAX_MOUSE_KEYS);

  return input_state->current_mouse_states[key];
}

v2 mouse_world_position(InputState *input_state)
{
    PlatformState *platform_state = get_engine_platform_state();
    RendererState *renderer_state = get_engine_renderer_state();

    v2 p = mouse_screen_position(platform_state);
    float screen_width = get_screen_width(platform_state);
    float screen_height = get_screen_height(platform_state);

    p.y = screen_height - p.y;

    v2 ndc =
    {
        (p.x / screen_width)  * 2.0f - 1.0f,
        (p.y / screen_height) * 2.0f - 1.0f,
    };

    return ndc_point_to_world(renderer_state, ndc);
}



InputState *init_input()
{
  InputState *input_state = (InputState *)my_allocate(sizeof(InputState));
  memset(input_state->current_key_states,  0, sizeof(bool) * MAX_KEYS);
  memset(input_state->previous_key_states, 0, sizeof(bool) * MAX_KEYS);
  memset(input_state->event_key_states,    0, sizeof(bool) * MAX_KEYS);

  memset(input_state->current_mouse_states,  0, sizeof(bool) * MAX_MOUSE_KEYS);
  memset(input_state->previous_mouse_states, 0, sizeof(bool) * MAX_MOUSE_KEYS);
  memset(input_state->event_mouse_states,    0, sizeof(bool) * MAX_MOUSE_KEYS);

  return input_state;
}

