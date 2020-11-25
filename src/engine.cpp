
#include "engine.h"
#include "platform.h"
#include "input.h"
#include "graphics.h"
#include "my_math.h"
#include "serialization.h"

#include "level.h"

// DEBUG
#include "imgui.h"
#include "examples/imgui_impl_win32.h"
#include "examples/imgui_impl_opengl3.h"




struct GameState
{
    bool running;

    double seconds_since_last_step;
    double last_loop_time;
    
    int num_levels;
    Level **levels;
    Level *playing_level;
    int current_score;

    // Maybe a list of unlocks (gun, grappling hook)
    bool gun_unlocked;
    bool hook_unlocked;
};
static GameState *game_state;
static const double TARGET_STEP_TIME = 0.01666; // In seconds
static const double MAX_STEPS_PER_LOOP = 10;



static void init_game_state()
{
    game_state = (GameState *)Platform::Memory::allocate(sizeof(GameState));

    game_state->running = true;
    game_state->seconds_since_last_step = 0.0;
    game_state->last_loop_time = 0.0;


    game_state->num_levels = 1;
    game_state->levels = (Level **)Platform::Memory::allocate(sizeof(Level *) * game_state->num_levels);
    for(int i = 0; i < game_state->num_levels; i++)
    {
        game_state->levels[i] = create_level();
    }
    game_state->playing_level = game_state->levels[0];

    game_state->current_score = 0;
    game_state->gun_unlocked = false;
    game_state->hook_unlocked = false;
}

static void init_imgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    
    // Setup Platform/Renderer bindings
    ImGui_ImplOpenGL3_Init("#version 440 core");
    ImGui_ImplWin32_Init(Platform::Window::handle());
}

static void imgui_begin_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

static void imgui_end_frame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void do_one_step(float time_step)
{
    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    
    ImGui::Begin("Debug");

    Input::read_input();


    // Update the in game level
    level_step(game_state->playing_level, time_step);

    if(ImGui::Button("Save to file"))
    {
        Serialization::Stream *stream = Serialization::make_stream();
        serialize_level(game_state->playing_level, stream);

        const char *path = "assets/data/levels/test";
        Platform::File *file = Platform::FileSystem::open(path, Platform::FileSystem::WRITE);
        Platform::FileSystem::write(file, Serialization::stream_data(stream), Serialization::stream_size(stream));
        Platform::FileSystem::close(file);

        Serialization::free_stream(stream);
    }

    if(ImGui::Button("Read from file"))
    {
        const char *path = "assets/data/levels/test";
        Platform::File *file = Platform::FileSystem::open(path, Platform::FileSystem::READ);
        Serialization::Stream *stream = Serialization::make_stream(Platform::FileSystem::size(file));
        Platform::FileSystem::read(file, Serialization::stream_data(stream), Platform::FileSystem::size(file));
        Platform::FileSystem::close(file);

        deserialize_level(game_state->playing_level, stream);

        Serialization::free_stream(stream);
    }



    ImGui::End();

    imgui_end_frame();
    Graphics::swap_frames();
}



void start_engine()
{
    Platform::init();
    Input::init();
    Graphics::init();

    init_game_state();
    init_imgui();


    seed_random(0);

    game_state->last_loop_time = Platform::time_since_start();
    while(game_state->running)
    {
        Platform::handle_os_events();

        double this_loop_time = Platform::time_since_start();
        double seconds_since_last_loop = this_loop_time - game_state->last_loop_time;
        game_state->last_loop_time = this_loop_time;
        game_state->seconds_since_last_step += seconds_since_last_loop;

        // If the target step_seconds has passed since last step, do one step
        if(game_state->seconds_since_last_step >= TARGET_STEP_TIME)
        {
            // You may have to update the engine more than once if more than one step time has passed
            int num_steps_to_do = floor(game_state->seconds_since_last_step / TARGET_STEP_TIME);

            if(num_steps_to_do <= MAX_STEPS_PER_LOOP)
            {
                for(int i = 0; i < num_steps_to_do; i++)
                {
                    // Move the engine forward in time by the target seconds
                    do_one_step(TARGET_STEP_TIME);
                }

                // Reset the timer
                game_state->seconds_since_last_step -= TARGET_STEP_TIME;
            }
            else
            {
                // Determinism is broken due to long frame times

                for(int i = 0; i < MAX_STEPS_PER_LOOP; i++)
                {
                    // Move the engine forward in time by the target seconds
                    do_one_step(TARGET_STEP_TIME);
                }

                // Reset the timer
                game_state->seconds_since_last_step = 0.0f;
            }
        }
        else
        {
            // Wait for a bit
        }
    }
}

void stop_engine()
{
    game_state->running = false;
}

