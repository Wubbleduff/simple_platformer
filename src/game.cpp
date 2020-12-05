
#include "game.h"
#include "platform.h"
#include "logging.h"
#include "data_structures.h"
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
    static const double TARGET_STEP_TIME;
    static const int MAX_STEPS_PER_LOOP;

    bool running;

    Network::GameMode network_mode;

    double seconds_since_last_step;
    double last_loop_time;



    DynamicArray<Players::Player *> players;

    int num_levels;
    Levels::Level **levels;
    Levels::Level *playing_level;
};
GameState *Game::instance = nullptr;
const double GameState::TARGET_STEP_TIME = 0.01666; // In seconds
const int GameState::MAX_STEPS_PER_LOOP = 10;



static void init_game(GameState **p_instance)
{
    GameState *&instance = *p_instance;

    instance = (GameState *)Platform::Memory::allocate(sizeof(GameState));

    instance->running = true;
    instance->seconds_since_last_step = 0.0;
    instance->last_loop_time = 0.0;

    instance->network_mode = Network::GameMode::OFFLINE;

    instance->players.init();

    Game::add_player();

    instance->num_levels = 1;
    instance->levels = (Levels::Level **)Platform::Memory::allocate(sizeof(Levels::Level *) * instance->num_levels);
    for(int i = 0; i < instance->num_levels; i++)
    {
        instance->levels[i] = Levels::create_level();
    }
    instance->playing_level = instance->levels[0];
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

static void serialize_game(GameState *instance, Serialization::Stream *stream)
{
    Levels::serialize_level(stream, instance->playing_level);
}

static void deserialize_game(GameState *instance, Serialization::Stream *stream)
{
    Levels::deserialize_level(stream, instance->playing_level);
}

static void switch_network_mode(GameState *instance, Network::GameMode mode)
{
    if(mode == Network::GameMode::OFFLINE)
    {
        instance->network_mode = Network::GameMode::OFFLINE;
    }

    if(mode == Network::GameMode::CLIENT)
    {
        instance->network_mode = Network::GameMode::CLIENT;
        Network::Client::init();
    }

    if(mode == Network::GameMode::SERVER)
    {
        instance->network_mode = Network::GameMode::SERVER;
        Network::Server::listen_for_client_connections(4242);
    }
}

static void step_as_offline(GameState *instance, float time_step)
{
    //Input::read_input();
    for(int i = 0; i < instance->players.size; i++)
    {
        Players::Player *player = instance->players[i];

        Players::read_actions(player);
    }

    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    Levels::draw_level(instance->playing_level);

    ImGui::Text("OFFLINE");

    if(ImGui::Button("Switch to client")) switch_network_mode(instance, Network::GameMode::CLIENT);
    if(ImGui::Button("Switch to server")) switch_network_mode(instance, Network::GameMode::SERVER);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}

static void step_as_client(GameState *instance, float time_step)
{
    //Input::read_input();

    Network::Client::send_input_state_to_server();

    Serialization::Stream *game_stream = Serialization::make_stream();
    bool read = Network::Client::read_game_state(game_stream);
    if(read)
    {
        deserialize_game(instance, game_stream);
    }
    Serialization::free_stream(game_stream);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("CLIENT");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, Network::GameMode::OFFLINE);
    if(ImGui::Button("Switch to server"))  switch_network_mode(instance, Network::GameMode::SERVER);

    if(ImGui::Button("Connect to server"))
    {
        Network::Client::connect_to_server("127.0.0.1", 4242);
    }

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}


static void step_as_server(GameState *instance, float time_step)
{
    Network::Server::read_client_input_states();
    //Input::read_input();

    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);


    Network::Server::accept_client_connections();

    Serialization::Stream *game_stream = Serialization::make_stream();
    serialize_game(instance, game_stream);
    Network::Server::broadcast_game(game_stream);
    Serialization::free_stream(game_stream);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("SERVER");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, Network::GameMode::OFFLINE);
    if(ImGui::Button("Switch to client"))  switch_network_mode(instance, Network::GameMode::CLIENT);

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();

}

static void do_one_step(GameState *instance, float time_step)
{
    switch(instance->network_mode)
    {
        case Network::GameMode::OFFLINE:
        {
            step_as_offline(instance, time_step);
            break;
        }
        case Network::GameMode::SERVER:
        {
            step_as_server(instance, time_step);
            break;
        }
        case Network::GameMode::CLIENT:
        {
            step_as_client(instance, time_step);
            break;
        }
    }
}

void Game::start()
{
    Platform::init();
    Log::init();
    GameConsole::init();
    Graphics::init();
    Network::init();

    init_game(&instance);
    init_imgui();

    Log::log_info("Hello, sir! %d, %s", 42, "Bye!");

    seed_random(0);

    instance->last_loop_time = Platform::time_since_start();
    while(instance->running)
    {
        Platform::handle_os_events();

        double this_loop_time = Platform::time_since_start();
        double seconds_since_last_loop = this_loop_time - instance->last_loop_time;
        instance->last_loop_time = this_loop_time;
        instance->seconds_since_last_step += seconds_since_last_loop;

        // If the target step_seconds has passed since last step, do one step
        if(instance->seconds_since_last_step >= GameState::TARGET_STEP_TIME)
        {
            // You may have to update the engine more than once if more than one step time has passed
            int num_steps_to_do = (int)(instance->seconds_since_last_step / GameState::TARGET_STEP_TIME);

            if(num_steps_to_do <= GameState::MAX_STEPS_PER_LOOP)
            {
                for(int i = 0; i < num_steps_to_do; i++)
                {
                    // Move the engine forward in time by the target seconds
                    do_one_step(instance, GameState::TARGET_STEP_TIME);

                }

                // Reset the timer
                instance->seconds_since_last_step -= GameState::TARGET_STEP_TIME * num_steps_to_do;
            }
            else
            {
                // Determinism is broken due to long frame times

                for(int i = 0; i < GameState::MAX_STEPS_PER_LOOP; i++)
                {
                    // Move the engine forward in time by the target seconds
                    do_one_step(instance, GameState::TARGET_STEP_TIME);
                }

                // Reset the timer
                instance->seconds_since_last_step = 0.0f;
            }
        }
        else
        {
            // Wait for a bit
        }
    }
}

void Game::stop()
{
    instance->running = false;
}

void Game::add_player()
{
}

