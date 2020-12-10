
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
#include <map>


using namespace GameMath;



struct Player
{
    bool current_actions[(int)Game::Players::Action::NUM_ACTIONS];

    virtual void read_actions() = 0;

    virtual void cleanup() = 0;
};



struct LocalPlayer : public Player
{
    void read_actions() override
    {
        current_actions[(int)Game::Players::Action::MOVE_RIGHT] = Platform::Input::key('D');
        current_actions[(int)Game::Players::Action::MOVE_LEFT] = Platform::Input::key('A');
        current_actions[(int)Game::Players::Action::JUMP] = Platform::Input::key_down(' ');
    }

    void cleanup() override {}
};

struct RemotePlayer : public Player
{
    Network::Connection *connection;

    void read_actions() override
    {
        if(connection == nullptr) return;

        Serialization::Stream *input_stream = Serialization::make_stream();

        Network::ReadResult result = connection->read_into_stream(input_stream);

        if(result == Network::ReadResult::CLOSED)
        {
            Network::disconnect(&connection);
            Serialization::free_stream(input_stream);
            return;
        }
        else if(result == Network::ReadResult::READY)
        {
            input_stream->move_to_beginning();
        }

        // TODO: Sanitize ...

        int i = 0;
        while(!input_stream->at_end())
        {
            int value;
            input_stream->read(&value);
            current_actions[i] = (value == 0) ? false : true;

            i++;
        }

        Serialization::free_stream(input_stream);
    }

    void cleanup() override
    {
        Network::disconnect(&connection);
    }
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
            DynamicArray<Game::PlayerID> remote_players;
        } server;
    };

    static int next_player_id;
    DynamicArray<Game::PlayerID> local_players;

    std::map<int, Player *> *players_map;
    

    int num_levels;
    Levels::Level **levels;
    Levels::Level *playing_level;
};
GameState *Game::instance = nullptr;
const double GameState::TARGET_STEP_TIME = 0.01666; // In seconds
const int GameState::MAX_STEPS_PER_LOOP = 10;
int GameState::next_player_id = 1;



static Player *get_player_from_id(GameState *instance, Game::PlayerID id)
{
    std::map<int, Player *>::iterator it = instance->players_map->find(id.id);
    if(it != instance->players_map->end())
    {
        return (*it).second;
    }

    return nullptr;
}

static void init_game(GameState **p_instance)
{
    GameState *&instance = *p_instance;

    instance = (GameState *)Platform::Memory::allocate(sizeof(GameState));

    instance->running = true;
    instance->seconds_since_last_step = 0.0;
    instance->last_loop_time = 0.0;

    instance->network_mode = GameState::NetworkMode::OFFLINE;

    instance->players_map = new std::map<int, Player *>();

    instance->local_players.init();

    Game::PlayerID player1_id = Game::Players::add();

    instance->num_levels = 1;
    instance->levels = (Levels::Level **)Platform::Memory::allocate(sizeof(Levels::Level *) * instance->num_levels);
    for(int i = 0; i < instance->num_levels; i++)
    {
        instance->levels[i] = Levels::create_level();
    }
    instance->playing_level = instance->levels[0];

    Levels::add_avatar(instance->playing_level, player1_id);
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
    for(int i = 0; i < instance->server.remote_players.size; i++)
    {
        Game::PlayerID id = instance->server.remote_players[i];
        RemotePlayer *player = (RemotePlayer *)get_player_from_id(instance, id);
        Network::Connection *& connection = player->connection;
        Network::disconnect(&connection);
    }
    instance->server.remote_players.clear();
    Network::stop_listening_for_client_connections();
    instance->server.remote_players.uninit();
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
        instance->server.remote_players.init();
        Network::listen_for_client_connections(4242);
    }
}

static void step_as_offline(GameState *instance, float time_step)
{
    for(int i = 0; i < instance->local_players.size; i++)
    {
        Game::PlayerID id = instance->local_players[i];
        Player *player = get_player_from_id(instance, id);
        player->read_actions();
    }

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
        Game::PlayerID new_id = Game::Players::add();

        Levels::add_avatar(instance->playing_level, new_id);
    }

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}

static void step_as_client(GameState *instance, float time_step)
{
    for(int i = 0; i < instance->local_players.size; i++)
    {
        Game::PlayerID id = instance->local_players[i];
        Player *player = get_player_from_id(instance, id);
        player->read_actions();
    }

    // Send local player inputs to server
    if(instance->client.server_connection)
    {
        // TODO: 1 client = 1 player for now...
        Game::PlayerID id = instance->local_players[0];
        Player *player = get_player_from_id(instance, id);
        Serialization::Stream *input_stream = Serialization::make_stream();
        for(int i = 0; i < (int)Game::Players::Action::NUM_ACTIONS; i++)
        {
            bool action = player->current_actions[i];
            int value = (action == 0) ? false : true;
            input_stream->write(value);
        }
        instance->client.server_connection->send_stream(input_stream);
        Serialization::free_stream(input_stream);
    }

    // Read server game state
    
    Network::Connection *connection_to_server = instance->client.server_connection;
    if(connection_to_server)
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        Network::ReadResult result = connection_to_server->read_into_stream(game_stream);
        
        if(result == Network::ReadResult::CLOSED)
        {
            Network::disconnect(&(instance->client.server_connection));
        }
        else if(result == Network::ReadResult::READY)
        {
            game_stream->move_to_beginning();
            deserialize_game(instance, game_stream);
        }

        Serialization::free_stream(game_stream);
    }
    else
    {
        Levels::step_level(instance->playing_level, time_step);
    }
    


    // Draw
    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    imgui_begin_frame();
    ImGui::Begin("Debug");

    ImGui::Text("CLIENT");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, GameState::NetworkMode::OFFLINE);
    if(ImGui::Button("Switch to server"))  switch_network_mode(instance, GameState::NetworkMode::SERVER);

    char address_string[16] = "127.0.0.1";
    ImGui::InputText("Address", address_string, sizeof(address_string), 0);
    if(ImGui::Button("Connect to server"))
    {
        instance->client.server_connection = Network::connect(address_string, 4242);
    }

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    imgui_end_frame();
    Graphics::swap_frames();
}


static void step_as_server(GameState *instance, float time_step)
{
    // Read local player inputs
    for(int i = 0; i < instance->local_players.size; i++)
    {
        Game::PlayerID id = instance->local_players[i];
        Player *player = get_player_from_id(instance, id);
        player->read_actions();
    }

    // Read remove player inputs
    for(int i = 0; i < instance->server.remote_players.size; i++)
    {
        Game::PlayerID id = instance->server.remote_players[i];
        Player *player = get_player_from_id(instance, id);
        player->read_actions();
    }

    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);

    DynamicArray<Network::Connection *> new_connections = Network::accept_client_connections();
    for(int i = 0; i < new_connections.size; i++)
    {
        Network::Connection *new_connection = new_connections[i];
        Game::PlayerID id = Game::Players::add_remote(new_connection);
    }
    new_connections.uninit();

    Serialization::Stream *game_stream = Serialization::make_stream();
    serialize_game(instance, game_stream);
    game_stream->move_to_beginning();
    for(int i = 0; i < instance->server.remote_players.size; i++)
    {
        Game::PlayerID id = instance->server.remote_players[i];
        RemotePlayer *player = (RemotePlayer *)get_player_from_id(instance, id); // What's a more type safe way of doing this?
        Network::Connection *connection_to_client = player->connection;

        if(connection_to_client == nullptr) continue;

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

    //seed_random(0);
    seed_random((int)(Platform::time_since_start() * 10000.0f));

    init_game(&instance);
    init_imgui();

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


static int get_next_player_id(GameState *instance)
{
    return instance->next_player_id++;
}
Game::PlayerID Game::Players::add()
{
    // TODO: What to do with new?
    LocalPlayer *new_player = new LocalPlayer;
    Platform::Memory::memset(new_player->current_actions, 0, sizeof(bool) * (int)Game::Players::Action::NUM_ACTIONS);

    int id = get_next_player_id(instance);

    (*instance->players_map)[id] = new_player;
    instance->local_players.push_back({ id });

    return { id };
}

Game::PlayerID Game::Players::add_remote(Network::Connection *connection)
{
    // TODO: What to do with new?
    RemotePlayer *new_player = new RemotePlayer;
    Platform::Memory::memset(new_player->current_actions, 0, sizeof(bool) * (int)Game::Players::Action::NUM_ACTIONS);

    int id = get_next_player_id(instance);

    (*instance->players_map)[id] = new_player;
    instance->server.remote_players.push_back({ id });

    return { id };
}

void Game::Players::remove(PlayerID id)
{
    instance->players_map->erase(id.id);
    //std::map<int, Player *>::iterator to_remove_it = instance->players_map->find(id.id);
}

bool Game::Players::action(PlayerID id, Action action)
{
    std::map<int, Player *>::iterator it = instance->players_map->find(id.id);
    if(it != instance->players_map->end())
    {
        Player *player = (*it).second;
        int index = (int)action;
        return player->current_actions[index];
    }

    return false;
}


