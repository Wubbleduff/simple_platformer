
#include "engine.h"
#include "platform.h"
#include "logging.h"
#include "input.h"
#include "game_console.h"
#include "graphics.h"
#include "game_math.h"
#include "serialization.h"
#include "network.h"

#include "levels.h"

// DEBUG
#include "imgui.h"



using namespace GameMath;


struct GameState
{
    bool running;

    Network::GameMode network_mode;

    double seconds_since_last_step;
    double last_loop_time;


    
    int num_levels;
    Levels::Level **levels;
    Levels::Level *playing_level;
    int current_score;

    // Maybe a list of unlocks (gun, grappling hook)
    bool gun_unlocked;
    bool hook_unlocked;
};
static GameState *game_state;
static const double TARGET_STEP_TIME = 0.01666; // In seconds
static const int MAX_STEPS_PER_LOOP = 10;



static void init_game_state()
{
    game_state = (GameState *)Platform::Memory::allocate(sizeof(GameState));

    game_state->running = true;
    game_state->seconds_since_last_step = 0.0;
    game_state->last_loop_time = 0.0;

    game_state->network_mode = Network::GameMode::OFFLINE;


    game_state->num_levels = 1;
    game_state->levels = (Levels::Level **)Platform::Memory::allocate(sizeof(Levels::Level *) * game_state->num_levels);
    for(int i = 0; i < game_state->num_levels; i++)
    {
        game_state->levels[i] = Levels::create_level();
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
    Graphics::ImGuiImplementation::init();
}

static void imgui_begin_frame()
{
    Graphics::ImGuiImplementation::new_frame();
    ImGui::NewFrame();
}

static void imgui_end_frame()
{
    ImGui::Render();
    Graphics::ImGuiImplementation::end_frame();
}

static void serialize_game_state(Serialization::Stream *stream)
{
    Levels::serialize_level(stream, game_state->playing_level);
}

static void deserialize_game_state(Serialization::Stream *stream)
{
    Levels::deserialize_level(stream, game_state->playing_level);
}

static void switch_network_mode(Network::GameMode mode)
{
    if(mode == Network::GameMode::OFFLINE)
    {
        game_state->network_mode = Network::GameMode::OFFLINE;
    }

    if(mode == Network::GameMode::CLIENT)
    {
        game_state->network_mode = Network::GameMode::CLIENT;
        Network::Client::init();
    }

    if(mode == Network::GameMode::SERVER)
    {
        game_state->network_mode = Network::GameMode::SERVER;
        Network::Server::listen_for_client_connections(4242);
    }
}

static void step_as_offline(float time_step)
{
    Input::read_input();

    // Update the in game level
    Levels::step_level(game_state->playing_level, time_step);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    Levels::draw_level(game_state->playing_level);

    ImGui::Text("OFFLINE");

    if(ImGui::Button("Switch to client")) switch_network_mode(Network::GameMode::CLIENT);
    if(ImGui::Button("Switch to server")) switch_network_mode(Network::GameMode::SERVER);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}

static void step_as_client(float time_step)
{
    Input::read_input();

    Network::Client::send_input_state_to_server();

    Serialization::Stream *game_state_stream = Serialization::make_stream();
    bool read = Network::Client::read_game_state(game_state_stream);
    if(read)
    {
        deserialize_game_state(game_state_stream);
    }
    Serialization::free_stream(game_state_stream);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("CLIENT");

    if(ImGui::Button("Switch to offline")) switch_network_mode(Network::GameMode::OFFLINE);
    if(ImGui::Button("Switch to server"))  switch_network_mode(Network::GameMode::SERVER);

    if(ImGui::Button("Connect to server"))
    {
        Network::Client::connect_to_server("127.0.0.1", 4242);
    }

    Levels::draw_level(game_state->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}


static void step_as_server(float time_step)
{
    Network::Server::read_client_input_states();
    Input::read_input();

    // Update the in game level
    Levels::step_level(game_state->playing_level, time_step);


    Network::Server::accept_client_connections();

    Serialization::Stream *game_state_stream = Serialization::make_stream();
    serialize_game_state(game_state_stream);
    Network::Server::broadcast_game_state(game_state_stream);
    Serialization::free_stream(game_state_stream);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("SERVER");

    if(ImGui::Button("Switch to offline")) switch_network_mode(Network::GameMode::OFFLINE);
    if(ImGui::Button("Switch to client"))  switch_network_mode(Network::GameMode::CLIENT);

    Levels::draw_level(game_state->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();

}

static void do_one_step(float time_step)
{
    switch(game_state->network_mode)
    {
        case Network::GameMode::OFFLINE:
        {
            step_as_offline(time_step);
            break;
        }
        case Network::GameMode::SERVER:
        {
            step_as_server(time_step);
            break;
        }
        case Network::GameMode::CLIENT:
        {
            step_as_client(time_step);
            break;
        }
    }
}

void start_engine()
{
    Platform::init();
    Log::init();
    GameConsole::init();
    Input::init();
    Graphics::init();
    Network::init();

    init_game_state();
    init_imgui();

    Log::log_info("Hello, sir! %d, %s", 42, "Bye!");

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
            int num_steps_to_do = (int)(game_state->seconds_since_last_step / TARGET_STEP_TIME);

            if(num_steps_to_do <= MAX_STEPS_PER_LOOP)
            {
                for(int i = 0; i < num_steps_to_do; i++)
                {
                    // Move the engine forward in time by the target seconds
                    do_one_step(TARGET_STEP_TIME);

                }

                // Reset the timer
                game_state->seconds_since_last_step -= TARGET_STEP_TIME * num_steps_to_do;
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

