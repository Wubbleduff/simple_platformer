
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
#include <array>
#include <map>
#include <algorithm>

// DEBUG
#include "imgui.h"



using namespace GameMath;



struct MenuState
{
    void step(float time_step);
    void draw();
    void cleanup();
};

struct GameState
{
   enum Mode
   {
        INVALID,
        MAIN_MENU,
        PLAYING_LEVEL,
        EDITING
    };
    Mode current_mode;
    Mode next_mode;

    GameInput::UID my_uid;
    unsigned int frame_number;
    std::vector<GameInput> inputs_this_frame;

    MenuState *menu_state; // For main menu, win/lose screen, etc. (What about pause menu? idk...)
    Level *playing_level;  // For when the player is playing the game

    void step(std::vector<GameInput> *inputs, GameInput::UID focus_uid, float time_step);
    void draw();
    void switch_game_mode(GameState::Mode mode);
    void init_for_main_menu(bool initting);
    void init_for_playing_level(bool initting);
    void check_for_mode_switch();
    void serialize_into(Serialization::Stream *stream, GameInput::UID uid);
    void deserialize_from(Serialization::Stream *stream);
};

struct Timeline
{
    static const int MAX_STEPS_PER_UPDATE;
    float seconds_since_last_step;
    float last_update_time;

    float step_frequency;

    int next_step_time_index;
    std::array<float, 120> step_times;

    void reset();
    void step_with_frequency(float freq);
    void update();
};
const int Timeline::MAX_STEPS_PER_UPDATE = 10;

struct ProgramState
{
    static const float TARGET_STEP_TIME;
    bool running;

    Timeline *timeline = nullptr;

    enum class NetworkMode
    {
        OFFLINE,
        CLIENT,
        SERVER,
    };
    NetworkMode network_mode;

    struct Client
    {
        static const float TIMEOUT;

        Network::Connection *server_connection;
        float connecting_timeout_timer = 0.0f;

        void connect_to_server(const char *ip_address, int port);
        void disconnect_from_server();
        bool is_connected();
        void update_connection(float time_step);
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
    void switch_network_mode(NetworkMode mode);
    void step_as_offline(float time_step);
    void step_as_client(float time_step);
    void step_as_server(float time_step);
    void do_one_step(float time_step);
    void shutdown();
};
ProgramState *Game::program_state = nullptr;
const float ProgramState::TARGET_STEP_TIME = 0.01666; // In seconds
static const int SERVER_PORT = 4242;
const float ProgramState::Client::TIMEOUT = 2.0f;



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



void GameState::check_for_mode_switch()
{
    if(current_mode != next_mode)
    {
        // Clean up previous state
        switch(current_mode)
        {
            case MAIN_MENU:
                init_for_main_menu(false);
                break;
            case PLAYING_LEVEL:
                init_for_playing_level(false);
                break;
            case EDITING:
                init_for_playing_level(false);
                break;
        }

        // Init next state
        switch(next_mode)
        {
            case MAIN_MENU:
                init_for_main_menu(true);
                break;
            case PLAYING_LEVEL:
                init_for_playing_level(true);
                break;
            case EDITING:
                init_for_playing_level(true);
                break;
        }

        current_mode = next_mode;
    }
}

void GameState::step(std::vector<GameInput> *inputs, GameInput::UID focus_uid, float time_step)
{
    check_for_mode_switch();

    inputs_this_frame = *inputs;
    my_uid = focus_uid;

    frame_number++;

    switch(current_mode)
    {
        case Mode::MAIN_MENU:
            menu_state->step(time_step);
            break;
        case Mode::PLAYING_LEVEL:
            playing_level->step(inputs_this_frame, time_step);
            break;
        case Mode::EDITING:
            playing_level->edit_step(time_step);
            break;
    }
}

void GameState::draw()
{
    switch(current_mode)
    {
    case Mode::MAIN_MENU:
        menu_state->draw();
        break;
    case Mode::PLAYING_LEVEL:
        playing_level->draw();
        break;
    case Mode::EDITING:
        playing_level->edit_draw();
        break;
    }
}

void GameState::serialize_into(Serialization::Stream *stream, GameInput::UID other_uid)
{
    assert(current_mode == PLAYING_LEVEL);

    stream->write(other_uid);
    stream->write(frame_number);
    stream->write((int)inputs_this_frame.size());
    for(GameInput &input : inputs_this_frame)
    {
        input.serialize_into(stream);
    }
    playing_level->serialize(stream);
}

void GameState::deserialize_from(Serialization::Stream *stream)
{
    assert(current_mode == PLAYING_LEVEL);

    stream->read(&my_uid);
    stream->read(&frame_number);
    int num_inputs;
    stream->read(&num_inputs);
    inputs_this_frame.resize(num_inputs);
    for(int i = 0; i < num_inputs; i++)
    {
        GameInput *target = &(inputs_this_frame[i]);
        target->deserialize_from(stream);
    }
    playing_level->deserialize(stream);
}



void GameState::switch_game_mode(GameState::Mode mode)
{
    next_mode = mode;
}

void GameState::init_for_main_menu(bool initting)
{
    if(initting)
    {
        // Initialize state for menu
        menu_state = new MenuState();
    }
    else
    {
        if(menu_state) menu_state->cleanup();
        delete menu_state;
        menu_state = nullptr;
    }
}

void GameState::init_for_playing_level(bool initting)
{
    if(initting)
    {
        // Initialize state for playing level
        my_uid = 0;
        frame_number = 0;
        inputs_this_frame.clear();
        playing_level = create_level(0);
    }
    else
    {
        destroy_level(playing_level);
        playing_level = nullptr;
    }
}

void MenuState::step(float time_step)
{
}

void MenuState::draw()
{
    ImGui::Begin("Main Menu");
    if(ImGui::Button("Start Game"))
    {
        Game::program_state->current_game_state->switch_game_mode(GameState::Mode::PLAYING_LEVEL);
    }
    if(ImGui::Button("Edit Game"))
    {
        Game::program_state->current_game_state->switch_game_mode(GameState::Mode::EDITING);
    }
    if(ImGui::Button("Quit Game"))
    {
        Game::stop();
    }
    ImGui::End();
}

void MenuState::cleanup()
{
}



void Timeline::reset()
{
    seconds_since_last_step = 0.0f;
    last_update_time = (float)Platform::time_since_start();

    step_frequency = 0.0f;

    next_step_time_index = 0;
}

void Timeline::step_with_frequency(float freq)
{
    step_frequency = freq;
}

void Timeline::update()
{
    float this_update_time = (float)Platform::time_since_start();
    float diff = this_update_time - last_update_time;
    last_update_time = this_update_time;
    seconds_since_last_step += diff;

    // If time to do a step (i.e. timeline has crossed a step boundary)
    if(seconds_since_last_step >= step_frequency)
    {
        // Check how many step boundaries were crossed
        // (check how many steps to do this update)
        int num_steps_to_do = (int)(seconds_since_last_step / step_frequency);

        // Check if the number of steps to do is lower than the limit
        // so we don't blackhole with our timeline
        if(num_steps_to_do <= MAX_STEPS_PER_UPDATE)
        {
            // Step the game forward n times
            for(int i = 0; i < num_steps_to_do; i++)
            {
                float step_start = Platform::time_since_start();
                Game::program_state->do_one_step(step_frequency);
                float step_end = Platform::time_since_start();

                float step_diff = step_end - step_start;
                step_times[next_step_time_index++] = step_diff;
                if(next_step_time_index >= step_times.size())
                {
                    next_step_time_index = 0;
                }
            }

            // Reset the timer
            seconds_since_last_step -= step_frequency * num_steps_to_do;
        }
        else
        {
            // There were more steps to do than allowed, only update
            // by the max number of steps to avoid a blackhole
            for(int i = 0; i < MAX_STEPS_PER_UPDATE; i++)
            {
                float step_start = Platform::time_since_start();
                Game::program_state->do_one_step(step_frequency);
                float step_end = Platform::time_since_start();

                float step_diff = step_end - step_start;
                step_times[next_step_time_index++] = step_diff;
                if(next_step_time_index >= step_times.size())
                {
                    next_step_time_index = 0;
                }
            }

            // Reset the timer to zero
            // (it doesn't make sense to only reduce it by step_frequency * num_steps_to_do
            // because we missed some boundaries anyways)
            seconds_since_last_step = 0.0f;

            Log::log_warning("Exceeded max steps per timeline update");
        }
    }
    else
    {
        // Timeline hasn't reached a step boundary, wait for a bit
        // Maybe Sleep() here? Busy waiting at the moment...
    }
}




void ProgramState::Client::connect_to_server(const char *ip_address, int port)
{
    server_connection = Network::connect(ip_address, port);
}

void ProgramState::Client::disconnect_from_server()
{
    Network::disconnect(&server_connection);
    // Reset the level's state
    Game::program_state->current_game_state->switch_game_mode(GameState::Mode::PLAYING_LEVEL);
}

bool ProgramState::Client::is_connected()
{
    if(server_connection == nullptr) return false;
    return server_connection->is_connected();
}

void ProgramState::Client::update_connection(float time_step)
{
    if(server_connection == nullptr) return;
    if(server_connection->is_connected()) return;

    Log::log_info("Connecting...");
    connecting_timeout_timer += time_step;
    if(connecting_timeout_timer >= TIMEOUT)
    {
        Log::log_info("Timeout connecting to server!");
        disconnect_from_server();
        connecting_timeout_timer = 0.0f;
        return;
    }

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




void ProgramState::step_as_offline(float time_step)
{
    // Build inputs
    Platform::Input::read_input();
    GameInput local_input = {};
    local_input.read_from_local();

    std::vector<GameInput> input_list = {local_input};

    current_game_state->step(&input_list, 0, time_step);

    // Do drawing
    {
        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        ImGui::Begin("Debug");

        current_game_state->draw();

        ImGui::Text("OFFLINE");

        if(ImGui::Button("Switch to client")) Game::program_state->switch_network_mode(ProgramState::NetworkMode::CLIENT);
        if(ImGui::Button("Switch to server")) Game::program_state->switch_network_mode(ProgramState::NetworkMode::SERVER);

        ImGui::Begin("Frame times");
#if 0
        for(int i = 0; i < Game::program_state->timeline->step_times.size(); i++)
        {
            float step_time = Game::program_state->timeline->step_times[i];

            ImGui::Text("Time: %f", step_time);
        }
#endif
        float avg = GameMath::average(Game::program_state->timeline->step_times.data(), Game::program_state->timeline->step_times.size());
        ImGui::Text("Average step time: %f", avg);
        ImGui::PlotHistogram("Step Times", Game::program_state->timeline->step_times.data(), Game::program_state->timeline->step_times.size(), 0, 0, 0.0f, 0.032f, ImVec2(512.0f, 256.0f));
        ImGui::End();

        GameConsole::draw();

        ImGui::End();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void ProgramState::step_as_client(float time_step)
{
    Game::program_state->client.update_connection(time_step);

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
#if CLIENT_PREDICTION
    // Step the game forward in time
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
            current_game_state->deserialize_from(game_stream);
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
            current_game_state->draw();
        }

        GameConsole::draw();

        ImGui::End();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void ProgramState::step_as_server(float time_step)
{
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

    // Step the game state using all inputs
    current_game_state->step(&this_frames_inputs, 0, time_step);

    // Broadcast the players and game state
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        for(ProgramState::Server::ClientConnection &client : Game::program_state->server.client_connections)
        {
            current_game_state->serialize_into(game_stream, client.uid);
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

        current_game_state->draw();

        GameConsole::draw();

        ImGui::End();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void ProgramState::do_one_step(float time_step)
{
    switch(network_mode)
    {
        case NetworkMode::OFFLINE:
        {
            step_as_offline(time_step);
            break;
        }
        case NetworkMode::CLIENT:
        {
            step_as_client(time_step);
            break;
        }
        case NetworkMode::SERVER:
        {
            step_as_server(time_step);
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

    timeline = new Timeline();

    network_mode = ProgramState::NetworkMode::OFFLINE;

    current_game_state = new GameState();
    current_game_state->current_mode = GameState::Mode::INVALID;
    current_game_state->switch_game_mode(GameState::Mode::MAIN_MENU);

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

    program_state->timeline->reset();
    program_state->timeline->step_with_frequency(ProgramState::TARGET_STEP_TIME);
    while(program_state->running)
    {
        // Check on OS events as frequently as possible
        Platform::handle_os_events();
        if(program_state->running == false) break;

        program_state->timeline->update();
    }

    // -----------------
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

void Game::exit_to_main_menu()
{
    program_state->current_game_state->switch_game_mode(GameState::Mode::MAIN_MENU);
    program_state->switch_network_mode(ProgramState::NetworkMode::OFFLINE);
}


