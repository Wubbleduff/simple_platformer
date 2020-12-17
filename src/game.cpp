
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

#include <vector>
#include <map>
#include <algorithm>

// DEBUG
#include "imgui.h"



using namespace GameMath;



struct Player
{
    bool current_actions[(int)Players::Action::NUM_ACTIONS];
};

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

    struct Client
    {
        Network::Connection *server_connection;
        std::vector<PlayerID> remote_players;
        std::map<PlayerID, PlayerID> remote_to_local_id_map;
    } client;

    struct Server
    {
        std::vector<PlayerID> remote_players;
        std::map<PlayerID, Network::Connection *> remote_player_connections;
    } server;
    
    struct PlayersState
    {
        PlayerID local_player;
        int next_player_id;
        std::map<PlayerID, Player *> all_players;
    } players_state;
    


    int num_levels;
    Levels::Level **levels;
    Levels::Level *playing_level;
};
GameState *Game::instance = nullptr;
const double GameState::TARGET_STEP_TIME = 0.01666; // In seconds
const int GameState::MAX_STEPS_PER_LOOP = 10;



static Player *player_from_id(GameState *instance, PlayerID id)
{
    auto it = instance->players_state.all_players.find(id);
    if(it != instance->players_state.all_players.end())
    {
        return it->second;
    }

    return nullptr;
}

static bool remote_id_known(GameState *instance, PlayerID remote_id)
{
    auto it = instance->client.remote_to_local_id_map.find(remote_id);
    return it != instance->client.remote_to_local_id_map.end();
}

static int get_next_player_id(GameState *instance)
{
    return instance->players_state.next_player_id++;
}

static PlayerID add_remote_player_as_client(GameState *instance, PlayerID remote_id)
{
    PlayerID id = get_next_player_id(instance);
    instance->players_state.all_players[id] = new Player();
    instance->client.remote_players.push_back(id);
    instance->client.remote_to_local_id_map[remote_id] = id;
    Levels::add_avatar(instance->playing_level, id);

    return id;
}

static void remove_remote_player_as_client(GameState *instance, PlayerID id_to_remove)
{
    Levels::remove_avatar(instance->playing_level, id_to_remove);

    for(auto it = instance->client.remote_to_local_id_map.begin();
            it != instance->client.remote_to_local_id_map.end(); ++it)
    {
        if(it->second == id_to_remove)
        {
            it = instance->client.remote_to_local_id_map.erase(it);
            break;
        }
    }
    instance->client.remote_players.erase(
        std::remove(instance->client.remote_players.begin(), instance->client.remote_players.end(), id_to_remove),
        instance->client.remote_players.end());

    delete instance->players_state.all_players[id_to_remove];
    instance->players_state.all_players.erase(id_to_remove);
}

static PlayerID add_remote_player_as_server(GameState *instance, Network::Connection *connection)
{
    PlayerID id = get_next_player_id(instance);
    instance->players_state.all_players[id] = new Player();
    instance->server.remote_players.push_back(id);
    instance->server.remote_player_connections[id] = connection;
    Levels::add_avatar(instance->playing_level, id);

    return id;
}

static void remove_remote_player_as_server(GameState *instance, PlayerID id_to_remove)
{
    Network::Connection *connection = instance->server.remote_player_connections[id_to_remove];
    Network::disconnect(&connection);

    Levels::remove_avatar(instance->playing_level, id_to_remove);
    instance->server.remote_player_connections.erase(id_to_remove);
    instance->server.remote_players.erase(
            std::remove(instance->server.remote_players.begin(), instance->server.remote_players.end(), id_to_remove),
        instance->server.remote_players.end());
    delete instance->players_state.all_players[id_to_remove];
    instance->players_state.all_players.erase(id_to_remove);
}

static void fix_game_state(GameState *instance, Serialization::Stream *stream)
{
    // Deserialize the game

    // Deserialize the player list
    int num_players;
    std::vector<PlayerID> players_seen;
    stream->read(&num_players);
    for(int i = 0; i < num_players; i++)
    {
        // Read remote id
        int remote_id;
        stream->read(&remote_id);

        PlayerID local_id;

        // Check if the player is known on the client
        bool remote_known = remote_id_known(instance, remote_id);
        if(!remote_known)
        {
            // New player connected on server
            local_id = add_remote_player_as_client(instance, remote_id);
        }
        else
        {
            local_id = instance->client.remote_to_local_id_map[remote_id];
        }

        Player *player = instance->players_state.all_players[local_id];
        players_seen.push_back(local_id);

        // Read actions
        for(int i = 0; i < (int)Players::Action::NUM_ACTIONS; i++)
        {
            int value;
            stream->read(&value);
            player->current_actions[i] = (value == 0) ? false : true;;
        }
    }
    std::vector<PlayerID> disconnected_players;
    for(PlayerID player_id : instance->client.remote_players)
    {
        auto seen_player_location = std::find(players_seen.begin(), players_seen.end(), player_id);
        if(seen_player_location == players_seen.end())
        {
            disconnected_players.push_back(player_id);
        }
    }
    while(disconnected_players.size() > 0)
    {
        remove_remote_player_as_client(instance, disconnected_players.back());
        disconnected_players.pop_back();
    }

    // Deserialize the level
    Levels::deserialize_level(stream, instance->playing_level);
}

static void disconnect_client(GameState *instance)
{
    while(instance->client.remote_players.size() > 0)
    {
        remove_remote_player_as_client(instance, instance->client.remote_players.back());
    }
    Network::disconnect(&instance->client.server_connection);
}

static void disconnect_server(GameState *instance)
{
    while(instance->server.remote_players.size() > 0)
    {
        remove_remote_player_as_server(instance, instance->server.remote_players.back());
    }
    Network::stop_listening_for_client_connections();
    Log::log_info("Server shutdown");
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
        Network::listen_for_client_connections(4242);
    }
}

static void read_local_input(Player *player)
{
    player->current_actions[(int)Players::Action::MOVE_RIGHT] = Platform::Input::key('D');
    player->current_actions[(int)Players::Action::MOVE_LEFT] = Platform::Input::key('A');
    player->current_actions[(int)Players::Action::JUMP] = Platform::Input::key_down(' ');
}

static bool read_remote_input(Player *player, Network::Connection *connection)
{
    assert(connection != nullptr);

    Serialization::Stream *input_stream = Serialization::make_stream();

    Network::ReadResult result = connection->read_into_stream(input_stream);

    if(result == Network::ReadResult::CLOSED)
    {
        //Network::disconnect(&connection);
        Serialization::free_stream(input_stream);
        return false;
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
        player->current_actions[i] = (value == 0) ? false : true;

        i++;
    }

    Serialization::free_stream(input_stream);

    return true;
}

static void step_as_offline(GameState *instance, float time_step)
{
    Platform::Input::read_input();

    PlayerID id = instance->players_state.local_player;
    Player *player = player_from_id(instance, id);
    read_local_input(player);

    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);

    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    Graphics::ImGuiImplementation::new_frame();
    ImGui::Begin("Debug");

    Levels::draw_level(instance->playing_level);

    ImGui::Text("OFFLINE");

    if(ImGui::Button("Switch to client")) switch_network_mode(instance, GameState::NetworkMode::CLIENT);
    if(ImGui::Button("Switch to server")) switch_network_mode(instance, GameState::NetworkMode::SERVER);

    GameConsole::draw();

    ImGui::End();
    Graphics::ImGuiImplementation::end_frame();
    Graphics::swap_frames();
}

static void step_as_client(GameState *instance, float time_step)
{
    Platform::Input::read_input();

    // Read the local player input
    PlayerID local_player_id = instance->players_state.local_player;
    Player *local_player = player_from_id(instance, local_player_id);
    read_local_input(local_player);

    // Send the local player's input to the server
    if(instance->client.server_connection)
    {
        Serialization::Stream *input_stream = Serialization::make_stream();

        // Serialize local player input
        for(int i = 0; i < (int)Players::Action::NUM_ACTIONS; i++)
        {
            bool action = local_player->current_actions[i];
            int value = (action == 0) ? false : true;
            input_stream->write(value);
        }

        instance->client.server_connection->send_stream(input_stream);
        Serialization::free_stream(input_stream);
    }

    // Step the game locally using given input
    Levels::step_level(instance->playing_level, time_step);

    // Read server's game state
    Network::Connection *connection_to_server = instance->client.server_connection;
    if(connection_to_server)
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        Network::ReadResult result = connection_to_server->read_into_stream(game_stream);
        
        if(result == Network::ReadResult::CLOSED)
        {
            disconnect_client(instance);
        }
        else if(result == Network::ReadResult::READY)
        {
            game_stream->move_to_beginning();

            // Fix the local game state with the server game state
            fix_game_state(instance, game_stream);
        }

        Serialization::free_stream(game_stream);
    }



    // Draw
    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    Graphics::ImGuiImplementation::new_frame();
    ImGui::Begin("Debug");

    ImGui::Text("CLIENT");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, GameState::NetworkMode::OFFLINE);
    if(ImGui::Button("Switch to server"))  switch_network_mode(instance, GameState::NetworkMode::SERVER);

    static char address_string[16] = "127.0.0.1";
    ImGui::InputText("Address", address_string, sizeof(address_string), 0);
    if(instance->client.server_connection == nullptr)
    {
        if(ImGui::Button("Connect to server"))
        {
            instance->client.server_connection = Network::connect(address_string, 4242);
        }
    }
    else
    {
        if(ImGui::Button("Disconnect from server"))
        {
            disconnect_client(instance);
        }
    }

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    Graphics::ImGuiImplementation::end_frame();
    Graphics::swap_frames();
}


static void step_as_server(GameState *instance, float time_step)
{
    Platform::Input::read_input();

    // Accept new connections to the server
    std::vector<Network::Connection *> new_connections = Network::accept_client_connections();
    for(int i = 0; i < new_connections.size(); i++)
    {
        Network::Connection *new_connection = new_connections[i];
        PlayerID id = add_remote_player_as_server(instance, new_connection);
    }

    // Read local player's input
    PlayerID local_player_id = instance->players_state.local_player;
    Player *local_player = player_from_id(instance, local_player_id);
    read_local_input(local_player);

    // Read remote player's input
    std::vector<PlayerID> disconnected_players;
    for(const std::pair<PlayerID, Network::Connection *> &pair : instance->server.remote_player_connections)
    {
        Player *remote_player = player_from_id(instance, pair.first);
        Network::Connection *connection = pair.second;
        bool still_connected = read_remote_input(remote_player, connection);
        if(!still_connected) disconnected_players.push_back(pair.first);
    }
    // Clean out disconnected connections
    while(disconnected_players.size() > 0)
    {
        remove_remote_player_as_server(instance, disconnected_players.back());
        disconnected_players.pop_back();
    }

    // Update the in game level
    Levels::step_level(instance->playing_level, time_step);

    // Broadcast the players and game state
    {
        Serialization::Stream *game_stream = Serialization::make_stream();

        // Send game state to each remote connection
        for(const std::pair<PlayerID, Network::Connection *> &send_to : instance->server.remote_player_connections)
        {
            PlayerID send_to_player_id = send_to.first;
            Network::Connection *send_to_connection = send_to.second;

            // Serialize all players EXCEPT the client's self
            // (the client should have a local player state)
            int num_players = instance->players_state.all_players.size();
            num_players -= 1; // Subtract one to account for not sending the client's self
            game_stream->write(num_players);
            for(const std::pair<PlayerID, Player *> &sending : instance->players_state.all_players)
            {
                PlayerID sending_player_id = sending.first;
                Player *sending_player = sending.second;
                // Skip Client's self
                if(send_to_player_id == sending_player_id) continue;
                // Write player ID
                game_stream->write((int)sending_player_id);
                // Write all player actions
                for(int i = 0; i < (int)Players::Action::NUM_ACTIONS; i++)
                {
                    bool action = sending_player->current_actions[i];
                    int value = (action == 0) ? false : true;
                    game_stream->write(value);
                }
            }

            Levels::serialize_level(game_stream, instance->playing_level);

            send_to_connection->send_stream(game_stream);
            game_stream->clear();
        }

        Serialization::free_stream(game_stream);
    }



    // Draw game
    Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
    Graphics::ImGuiImplementation::new_frame();
    ImGui::Begin("Debug");

    ImGui::Text("SERVER");

    if(ImGui::Button("Switch to offline")) switch_network_mode(instance, GameState::NetworkMode::OFFLINE);
    if(ImGui::Button("Switch to client"))  switch_network_mode(instance, GameState::NetworkMode::CLIENT);

    Levels::draw_level(instance->playing_level);

    GameConsole::draw();

    ImGui::End();
    Graphics::ImGuiImplementation::end_frame();
    Graphics::swap_frames();

}

static void do_one_step(GameState *instance, float time_step)
{
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

static void init_game(GameState **p_instance)
{
    GameState *&instance = *p_instance;

    instance = new GameState;

    instance->running = true;
    instance->seconds_since_last_step = 0.0;
    instance->last_loop_time = 0.0;

    instance->network_mode = GameState::NetworkMode::OFFLINE;

    instance->players_state.next_player_id = { 0 };

    instance->num_levels = 1;
    instance->levels = new Levels::Level *[instance->num_levels]();
    for(int i = 0; i < instance->num_levels; i++)
    {
        instance->levels[i] = Levels::create_level();
    }
    instance->playing_level = instance->levels[0];


    
    // Add the local player
    instance->players_state.local_player = 0;
    instance->players_state.all_players[instance->players_state.local_player] = new Player();
    Levels::add_avatar(instance->playing_level, instance->players_state.local_player);
    instance->players_state.next_player_id = 1;
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




bool Players::action(PlayerID id, Action action)
{
    std::map<PlayerID, Player *>::iterator it = Game::instance->players_state.all_players.find(id);
    if(it != Game::instance->players_state.all_players.end())
    {
        Player *player = it->second;
        int index = (int)action;
        return player->current_actions[index];
    }

    return false;
}

PlayerID Players::remote_to_local_player_id(PlayerID remote_id)
{
    std::map<PlayerID, PlayerID>::iterator it = Game::instance->client.remote_to_local_id_map.find(remote_id);
    if(it == Game::instance->client.remote_to_local_id_map.end())
    {
        return -1;
    }
    return it->second;
}


