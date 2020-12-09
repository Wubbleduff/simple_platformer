
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



struct Player
{
    bool current_actions[(int)Game::Players::Action::NUM_ACTIONS];

    virtual void read_actions() = 0;
};

struct LocalPlayer : public Player
{
    void read_actions() override
    {
        current_actions[(int)Game::Players::Action::MOVE_RIGHT] = Platform::Input::key('D');
        current_actions[(int)Game::Players::Action::MOVE_LEFT] = Platform::Input::key('A');
        current_actions[(int)Game::Players::Action::JUMP] = Platform::Input::key_down(' ');
    }
};

struct RemotePlayer : public Player
{
    /*
       Network::Connection *connection_to_client;

    void read_actions() override
    {
        Serialization::Stream *input_stream = Serialization::make_stream();
        Network::read_data_into_stream(connection_to_client, input_stream);
        input_stream->reset();

        // TODO: Sanitize ...

        while(!input_stream->at_end())
        {
            int value;
            input_stream->read(&value);
            current_actions[i] = (value == 0) ? false : true;
        }
    }
    */
};

/*
struct AiPlayer : public Player
{
    void read_actions() override;
}
*/

struct GameState
{
    static const double TARGET_STEP_TIME;
    static const int MAX_STEPS_PER_LOOP;

    bool running;

    double seconds_since_last_step;
    double last_loop_time;



    enum class NetworkMode
    {
        OFFLINE,
        CLIENT,
        SERVER,
    };
    NetworkMode network_mode;
    union
    {
        struct Client
        {
            Network::Connection *server_connection;
        } client;

        struct Server
        {
            DynamicArray<Network::Connection *> client_connections;
        } server;
    };



    DynamicArray<Player *> players;

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

    instance->network_mode = GameState::NetworkMode::OFFLINE;

    instance->players.init();

    int player1_id = Game::Players::add();

    instance->num_levels = 1;
    instance->levels = (Levels::Level **)Platform::Memory::allocate(sizeof(Levels::Level *) * instance->num_levels);
    for(int i = 0; i < instance->num_levels; i++)
    {
        instance->levels[i] = Levels::create_level();
        Levels::add_avatar(instance->levels[i], player1_id);
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

static void disconnect_client(GameState *instance)
{
    Network::disconnect(&instance->client.server_connection);
}

static void disconnect_server(GameState *instance)
{
    for(int i = 0; i < instance->server.client_connections.size; i++)
    {
        Network::disconnect(&(instance->server.client_connections[i]));
    }
    instance->server.client_connections.clear();
    Network::stop_listening_for_client_connections();
    instance->server.client_connections.uninit();
}

static void switch_network_mode(GameState *instance, GameState::NetworkMode mode)
{
    if(mode == GameState::NetworkMode::OFFLINE)
    {
        if(instance->network_mode == GameState::NetworkMode::CLIENT)
        {
            disconnect_client(instance);
        }
        if(instance->network_mode == GameState::NetworkMode::SERVER)
        {
            disconnect_server(instance);
        }
        instance->network_mode = GameState::NetworkMode::OFFLINE;
    }

    if(mode == GameState::NetworkMode::CLIENT)
    {
        if(instance->network_mode == GameState::NetworkMode::SERVER)
        {
            disconnect_server(instance);
        }
        instance->network_mode = GameState::NetworkMode::CLIENT;
        instance->client.server_connection = nullptr;
    }

    if(mode == GameState::NetworkMode::SERVER)
    {
        if(instance->network_mode == GameState::NetworkMode::CLIENT)
        {
            disconnect_client(instance);
        }
        instance->network_mode = GameState::NetworkMode::SERVER;
        instance->server.client_connections.init();
        Network::listen_for_client_connections(4242);
    }
}

static void step_as_offline(GameState *instance, float time_step)
{
    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    Levels::draw_level(instance->playing_level);

    ImGui::Text("OFFLINE");

    if(ImGui::Button("Switch to client")) switch_network_mode(instance, GameState::NetworkMode::CLIENT);
    if(ImGui::Button("Switch to server")) switch_network_mode(instance, GameState::NetworkMode::SERVER);

    if(ImGui::Button("Add player"))
    {
        int new_id = Game::Players::add();

        Levels::add_avatar(instance->playing_level, new_id);
    }

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}

static void step_as_client(GameState *instance, float time_step)
{
    Serialization::Stream *game_stream = Serialization::make_stream();
    Network::Connection *connection_to_server = instance->client.server_connection;
    if(connection_to_server)
    {
        Network::ReadResult result = connection_to_server->read_into_stream(game_stream);
        if(result == Network::ReadResult::READY)
        {
            game_stream->reset();
            deserialize_game(instance, game_stream);
        }
    }
    Serialization::free_stream(game_stream);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("CLIENT");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, GameState::NetworkMode::OFFLINE);
    if(ImGui::Button("Switch to server"))  switch_network_mode(instance, GameState::NetworkMode::SERVER);

    if(ImGui::Button("Connect to server"))
    {
        instance->client.server_connection = Network::connect("127.0.0.1", 4242);
    }

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}


static void step_as_server(GameState *instance, float time_step)
{
    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);

    DynamicArray<Network::Connection *> &clients = instance->server.client_connections;
    Network::accept_client_connections(&clients);

    Serialization::Stream *game_stream = Serialization::make_stream();
    serialize_game(instance, game_stream);
    game_stream->reset();
    for(int i = 0; i < clients.size; i++)
    {
        Network::Connection *connection_to_client = clients[i];

        connection_to_client->send_stream(game_stream);
    }
    Serialization::free_stream(game_stream);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("SERVER");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, GameState::NetworkMode::OFFLINE);
    if(ImGui::Button("Switch to client"))  switch_network_mode(instance, GameState::NetworkMode::CLIENT);

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();

}

static void do_one_step(GameState *instance, float time_step)
{
    // Common step functions
    Platform::Input::read_input();
    for(int i = 0; i < instance->players.size; i++)
    {
        Player *player = instance->players[i];
        player->read_actions();
    }

    switch(instance->network_mode)
    {
        case GameState::NetworkMode::OFFLINE:
        {
            step_as_offline(instance, time_step);
            break;
        }
        case GameState::NetworkMode::SERVER:
        {
            step_as_server(instance, time_step);
            break;
        }
        case GameState::NetworkMode::CLIENT:
        {
            step_as_client(instance, time_step);
            break;
        }
    }
}

static void shutdown_game(GameState *instance)
{
    switch(instance->network_mode)
    {
        case GameState::NetworkMode::OFFLINE:
        {
            break;
        }

        case GameState::NetworkMode::CLIENT:
        {
            disconnect_client(instance);
            break;
        }

        case GameState::NetworkMode::SERVER:
        {
            disconnect_server(instance);
            break;
        }
    }

    Graphics::ImGuiImplementation::shutdown();
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

    seed_random(0);

    instance->last_loop_time = Platform::time_since_start();
    while(instance->running)
    {
        Platform::handle_os_events();

        if(instance->running == false) break;

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

    shutdown_game(instance);
}

void Game::stop()
{
    instance->running = false;
}



Game::PlayerID Game::Players::add()
{
    // TODO: What to do with new?
    LocalPlayer *new_player = new LocalPlayer;
    Platform::Memory::memset(new_player->current_actions, 0, sizeof(bool) * (int)Game::Players::Action::NUM_ACTIONS);
    instance->players.push_back(new_player);
    return instance->players.size - 1;
}

void Game::Players::remove(PlayerID id)
{
    // TODO
}

bool Game::Players::action(PlayerID id, Action action)
{
    Player *player = instance->players[id];
    int index = (int)action;
    return player->current_actions[index];
}


