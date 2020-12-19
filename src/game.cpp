
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



struct GameState
{
    GameInput::UID my_uid;
    unsigned int frame_number;
    std::vector<GameInput> inputs;
    Levels::Level *playing_level;

    void step_as_offline(float time_step);
    void step_as_client(float time_step);
    void step_as_server(float time_step);
    void reset_to_default();
    void serialize_into(Serialization::Stream *stream, GameInput::UID uid);
    void deserialize_from(Serialization::Stream *stream);
};

struct ProgramState
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

        void connect_to_server(const char *ip_address, int port);
        void disconnect_from_server();
        bool is_connected();
        void update_connection();
    } client;

    struct Server
    {
        struct ClientConnection
        {
            GameInput::UID uid = 0;
            Network::Connection *connection = nullptr;
        };
        std::vector<ClientConnection> client_connections;
        GameInput::UID next_input_uid = 1;

        bool startup(int port);
        void shutdown();
        void add_client_connection(Network::Connection *connection);
        void remove_disconnected_clients();
    } server;

    GameState *current_game_state;



    void init();
    void do_one_step(float time_step);
    void switch_network_mode(NetworkMode mode);
    void shutdown();
};
ProgramState *Game::program_state = nullptr;
const double ProgramState::TARGET_STEP_TIME = 0.01666; // In seconds
const int ProgramState::MAX_STEPS_PER_LOOP = 10;
static const int SERVER_PORT = 4242;



bool GameInput::action(Action action)
{
    return current_actions[(int)action];
}

void GameInput::serialize_into(Serialization::Stream *stream)
{
    for(int i = 0; i < (int)Action::NUM_ACTIONS; i++)
    {
        stream->write((int)current_actions[i]);
    }
}

void GameInput::deserialize_from(Serialization::Stream *stream)
{
    for(int i = 0; i < (int)Action::NUM_ACTIONS; i++)
    {
        int value;
        stream->read(&value);
        current_actions[i] = (value == 0) ? false : true;
    }
}

void GameInput::read_from_local()
{
    current_actions[(int)Action::MOVE_RIGHT] = Platform::Input::key('D');
    current_actions[(int)Action::MOVE_LEFT] = Platform::Input::key('A');
    current_actions[(int)Action::JUMP] = Platform::Input::key_down(' ');
}

void GameInput::read_from_connection(Network::Connection **connection)
{
    assert(*connection != nullptr);

    Serialization::Stream *input_stream = Serialization::make_stream();

    Network::ReadResult result = (*connection)->read_into_stream(input_stream);

    if(result == Network::ReadResult::CLOSED)
    {
        Network::disconnect(connection);
        Serialization::free_stream(input_stream);
        return;
    }
    else if(result == Network::ReadResult::READY)
    {
        input_stream->move_to_beginning();

        // TODO: Sanitize ...

        deserialize_from(input_stream);
    }

    Serialization::free_stream(input_stream);
}



void GameState::step_as_offline(float time_step)
{
    my_uid = 0;
    frame_number++;

    Platform::Input::read_input();
    GameInput local_input = {};
    local_input.read_from_local();
    inputs.resize(1);
    inputs[0] = local_input;

    // Update the in game level
    Levels::step_level(inputs, playing_level, time_step);



    // Draw
    {
        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        ImGui::Begin("Debug");

        Levels::draw_level(playing_level);

        ImGui::Text("OFFLINE");

        if(ImGui::Button("Switch to client")) Game::program_state->switch_network_mode(ProgramState::NetworkMode::CLIENT);
        if(ImGui::Button("Switch to server")) Game::program_state->switch_network_mode(ProgramState::NetworkMode::SERVER);

        GameConsole::draw();

        ImGui::End();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void GameState::step_as_client(float time_step)
{
    Game::program_state->client.update_connection();

    // Send the local player's input to the server
    Platform::Input::read_input();
    if(Game::program_state->client.is_connected())
    {
        Serialization::Stream *input_stream = Serialization::make_stream();

        GameInput local_input;
        local_input.read_from_local();
        local_input.serialize_into(input_stream);
        Game::program_state->client.server_connection->send_stream(input_stream);

        Serialization::free_stream(input_stream);
    }

#define CLIENT_PREDICTION 0
    // Step the game forward in time
#if CLIENT_PREDICTION
    // ...
#endif

    // Read server's game state
    if(Game::program_state->client.is_connected())
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        Network::ReadResult result = Game::program_state->client.server_connection->read_into_stream(game_stream);
        
        if(result == Network::ReadResult::CLOSED)
        {
            Game::program_state->client.disconnect_from_server();
        }
        else if(result == Network::ReadResult::READY)
        {
            game_stream->move_to_beginning();

            // Fix the local game state with the server game state
            // This should happen in the future
#if CLIENT_PREDICTION
            GameState *servers_game_state = new GameState();
            deserialize_game_state(game_stream, servers_game_state);
            fix_game_state(game_state_list, servers_game_state);
            delete server_game_state;
#endif

            // For now, we'll just be a dumb-client
            deserialize_from(game_stream);
        }

        Serialization::free_stream(game_stream);
    }



    // Draw
    {
        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        ImGui::Begin("Debug");

        ImGui::Text("CLIENT");

        if(ImGui::Button("Switch to offline")) Game::program_state->switch_network_mode(ProgramState::NetworkMode::OFFLINE);
        if(ImGui::Button("Switch to server"))  Game::program_state->switch_network_mode(ProgramState::NetworkMode::SERVER);

        bool game_state_still_valid = true;
        static char address_string[16] = "127.0.0.1";
        ImGui::InputText("Address", address_string, sizeof(address_string), 0);
        if(Game::program_state->client.is_connected())
        {
            if(ImGui::Button("Disconnect from server"))
            {
                Game::program_state->client.disconnect_from_server();
                game_state_still_valid = false;
            }
        }
        else
        {
            
            if(ImGui::Button("Connect to server"))
            {
                Game::program_state->client.connect_to_server(address_string, SERVER_PORT);
            }
        }

        if(game_state_still_valid)
        {
            Levels::draw_level(playing_level);
        }

        GameConsole::draw();

        ImGui::End();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void GameState::step_as_server(float time_step)
{
    my_uid = 0;
    frame_number++;

    // Accept new connections to the server
    std::vector<Network::Connection *> new_connections = Network::accept_client_connections();
    for(int i = 0; i < new_connections.size(); i++)
    {
        Network::Connection *new_connection = new_connections[i];
        Game::program_state->server.add_client_connection(new_connection);
    }

    // Prepare for reading input
    Platform::Input::read_input();
    std::vector<GameInput> this_frames_inputs;

    // Read local player's input
    GameInput local_input = {};
    local_input.read_from_local();
    this_frames_inputs.push_back(local_input);

    // Read remote player's input
    for(ProgramState::Server::ClientConnection &client : Game::program_state->server.client_connections)
    {
        GameInput remote_input;
        remote_input.uid = client.uid;
        remote_input.read_from_connection(&(client.connection));
        if(client.connection != nullptr)
        {
            this_frames_inputs.push_back(remote_input);
        }
    }
    // Clean out disconnected connections
    Game::program_state->server.remove_disconnected_clients();

    // All game input should have been read
    // Record all of this frame's inputs
    inputs = this_frames_inputs;

    // Update the level using game input
    Levels::step_level(inputs, playing_level, time_step);

    // Broadcast the players and game state
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        for(ProgramState::Server::ClientConnection &client : Game::program_state->server.client_connections)
        {
            serialize_into(game_stream, client.uid);
            client.connection->send_stream(game_stream);
            game_stream->clear();
        }
        Serialization::free_stream(game_stream);
    }



    // Draw game
    {
        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        ImGui::Begin("Debug");

        ImGui::Text("SERVER");

        if(ImGui::Button("Switch to offline")) Game::program_state->switch_network_mode(ProgramState::NetworkMode::OFFLINE);
        if(ImGui::Button("Switch to client"))  Game::program_state->switch_network_mode(ProgramState::NetworkMode::CLIENT);

        Levels::draw_level(playing_level);

        GameConsole::draw();

        ImGui::End();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void GameState::reset_to_default()
{
    my_uid = 0;
    frame_number = 0;
    inputs.clear();

    Levels::destroy_level(playing_level);
    playing_level = Levels::create_level();
}

void GameState::serialize_into(Serialization::Stream *stream, GameInput::UID other_uid)
{
    stream->write(other_uid);
    stream->write(frame_number);
    stream->write((int)inputs.size());
    for(GameInput &input : inputs)
    {
        input.serialize_into(stream);
    }
    Levels::serialize_level(stream, playing_level);
}

void GameState::deserialize_from(Serialization::Stream *stream)
{
    stream->read(&my_uid);
    stream->read(&frame_number);
    int num_inputs;
    stream->read(&num_inputs);
    inputs.resize(num_inputs);
    for(int i = 0; i < num_inputs; i++)
    {
        GameInput *target = &(inputs[i]);
        target->deserialize_from(stream);
    }
    Levels::deserialize_level(stream, playing_level);
}



void ProgramState::Client::connect_to_server(const char *ip_address, int port)
{
    server_connection = Network::connect(ip_address, port);
}

void ProgramState::Client::disconnect_from_server()
{
    Network::disconnect(&server_connection);
    Game::program_state->current_game_state->reset_to_default();
}
bool ProgramState::Client::is_connected()
{
    if(server_connection == nullptr) return false;
    return server_connection->is_connected();
}

void ProgramState::Client::update_connection()
{
    if(server_connection == nullptr) return;
    server_connection->check_on_connection_status();
}




bool ProgramState::Server::startup(int port)
{
    bool success = Network::listen_for_client_connections(port);
    return success;
}

void ProgramState::Server::shutdown()
{
    for(ClientConnection &client : client_connections)
    {
        Network::disconnect(&(client.connection));
    }
    client_connections.clear();
    Network::stop_listening_for_client_connections();
    Log::log_info("Server shutdown");
}

void ProgramState::Server::add_client_connection(Network::Connection *connection)
{
    client_connections.push_back( {next_input_uid, connection} );
    next_input_uid++;
}

void ProgramState::Server::remove_disconnected_clients()
{
    auto it = std::remove_if(client_connections.begin(), client_connections.end(),
            [](const ClientConnection &client) { return client.connection == nullptr; }
            );
    client_connections.erase(it, client_connections.end());
}




void ProgramState::do_one_step(float time_step)
{
    switch(network_mode)
    {
        case NetworkMode::OFFLINE:
        {
            current_game_state->step_as_offline(time_step);
            break;
        }
        case NetworkMode::SERVER:
        {
            current_game_state->step_as_server(time_step);
            break;
        }
        case NetworkMode::CLIENT:
        {
            current_game_state->step_as_client(time_step);
            break;
        }
    }
}

void ProgramState::switch_network_mode(NetworkMode mode)
{
    if(mode == NetworkMode::OFFLINE)
    {
        if(network_mode == NetworkMode::CLIENT)
        {
            client.disconnect_from_server();
        }
        if(network_mode == NetworkMode::SERVER)
        {
            server.shutdown();
        }
        network_mode = NetworkMode::OFFLINE;
    }

    if(mode == NetworkMode::CLIENT)
    {
        if(network_mode == NetworkMode::SERVER)
        {
            server.shutdown();
        }
        network_mode = NetworkMode::CLIENT;
        client.server_connection = nullptr;
    }

    if(mode == NetworkMode::SERVER)
    {
        if(network_mode == NetworkMode::CLIENT)
        {
            client.disconnect_from_server();
        }
        bool success = server.startup(SERVER_PORT);
        if(success)
        {
            network_mode = NetworkMode::SERVER;
        }
    }
}

void ProgramState::shutdown()
{
    switch(network_mode)
    {
        case NetworkMode::OFFLINE:
        {
            break;
        }

        case NetworkMode::CLIENT:
        {
            client.disconnect_from_server();
            break;
        }

        case NetworkMode::SERVER:
        {
            server.shutdown();
            break;
        }
    }

    Graphics::ImGuiImplementation::shutdown();
}

void ProgramState::init()
{
    running = true;
    seconds_since_last_step = 0.0;
    last_loop_time = 0.0;

    network_mode = ProgramState::NetworkMode::OFFLINE;

    current_game_state = new GameState();
    current_game_state->reset_to_default();
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

    program_state = new ProgramState();
    program_state->init();

    program_state->last_loop_time = Platform::time_since_start();
    while(program_state->running)
    {
        Platform::handle_os_events();

        if(program_state->running == false) break;

        double this_loop_time = Platform::time_since_start();
        double seconds_since_last_loop = this_loop_time - program_state->last_loop_time;
        program_state->last_loop_time = this_loop_time;
        program_state->seconds_since_last_step += seconds_since_last_loop;

        // If the target step_seconds has passed since last step, do one step
        if(program_state->seconds_since_last_step >= ProgramState::TARGET_STEP_TIME)
        {
            // You may have to update the engine more than once if more than one step time has passed
            int num_steps_to_do = (int)(program_state->seconds_since_last_step / ProgramState::TARGET_STEP_TIME);

            if(num_steps_to_do <= ProgramState::MAX_STEPS_PER_LOOP)
            {
                for(int i = 0; i < num_steps_to_do; i++)
                {
                    // Move the engine forward in time by the target seconds
                    program_state->do_one_step(ProgramState::TARGET_STEP_TIME);
                }

                // Reset the timer
                program_state->seconds_since_last_step -= ProgramState::TARGET_STEP_TIME * num_steps_to_do;
            }
            else
            {
                // Frame took too long
                for(int i = 0; i < ProgramState::MAX_STEPS_PER_LOOP; i++)
                {
                    // Move the engine forward in time by the target seconds
                    program_state->do_one_step(ProgramState::TARGET_STEP_TIME);
                }

                // Reset the timer
                program_state->seconds_since_last_step = 0.0f;
            }
        }
        else
        {
            // Wait for a bit
        }
    }

    program_state->shutdown();
}

void Game::stop()
{
    program_state->running = false;
}

GameInput::UID Game::get_my_uid()
{
    return program_state->current_game_state->my_uid;
}

