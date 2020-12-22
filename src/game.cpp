
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



// I want the player to be able to move between a main menu, playing the game, pause menu, win/lose menu, etc...
// I'm lost when thinking about where to put this state: under Program State or Game State
// The program state holds information about how to handle game states and step them from one to the next
// The game state holds information about input into the game and how to generate the next game state
// i.e. the program state is the "timeline" of the game states
// The main menu could be a particular "game state" that (maybe) updates on a different timeline
// Pause menus can be built into the normal game states
// These different game states will hold different information (The main menu shouldn't have a level)
// The different game states should use the same UI tools for drawing UI
struct MenuState
{
    struct Button
    {
        v2 position;
        v2 full_extents;
        v4 color;

        void (*on_click)(void) = nullptr;
    };

    std::vector<Button *> buttons;



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
        LOSE_MENU,
        WIN_MENU
    };
    Mode current_mode;
    Mode next_mode;

    GameInput::UID my_uid;
    unsigned int frame_number;
    std::vector<GameInput> inputs_this_frame;

    MenuState *menu_state;        // For main menu, win/lose screen, etc. (What about pause menu? idk...)
    Levels::Level *playing_level; // For when the player is playing the game

    void step(std::vector<GameInput> *inputs, GameInput::UID focus_uid, float time_step);
    void draw();
    void switch_game_mode(GameState::Mode mode);
    void init_for_main_menu(bool initting);
    void init_for_playing_level(bool initting);
    void check_for_mode_switch();
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
const double ProgramState::TARGET_STEP_TIME = 0.01666; // In seconds
const int ProgramState::MAX_STEPS_PER_LOOP = 10;
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
            Levels::step_level(inputs_this_frame, playing_level, time_step);
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
        Levels::draw_level(playing_level);
        break;
    }
}

void GameState::serialize_into(Serialization::Stream *stream, GameInput::UID other_uid)
{
    assert(current_mode == PLAYING_LEVEL, "Need to be in a level to serialize game state!");

    stream->write(other_uid);
    stream->write(frame_number);
    stream->write((int)inputs_this_frame.size());
    for(GameInput &input : inputs_this_frame)
    {
        input.serialize_into(stream);
    }
    Levels::serialize_level(stream, playing_level);
}

void GameState::deserialize_from(Serialization::Stream *stream)
{
    assert(current_mode == PLAYING_LEVEL, "Need to be in a level to deserialize game state!");

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
    Levels::deserialize_level(stream, playing_level);
}



void GameState::switch_game_mode(GameState::Mode mode)
{
    next_mode = mode;
}

// TODO: Move these somewhere nice :)
static void start_clicked() { Game::program_state->current_game_state->switch_game_mode(GameState::Mode::PLAYING_LEVEL); }
static void stop_clicked()
{
    Game::stop();
}
void GameState::init_for_main_menu(bool initting)
{
    if(initting)
    {
        // Initialize state for menu
        menu_state = new MenuState();

        MenuState::Button *start_button = new MenuState::Button();
        start_button->position = v2(-1.0f, 0.0f);
        start_button->full_extents = v2(1.0f, 1.0f);
        start_button->color = v4(0.0f, 1.0f, 0.0f, 1.0f);
        start_button->on_click = start_clicked;

        MenuState::Button *quit_button = new MenuState::Button();
        quit_button->position = v2(1.0f, 0.0f);
        quit_button->full_extents = v2(1.0f, 1.0f);
        quit_button->color = v4(1.0f, 0.0f, 0.0f, 1.0f);
        quit_button->on_click = stop_clicked;

        menu_state->buttons.push_back(start_button);
        menu_state->buttons.push_back(quit_button);
    }
    else
    {
        if(menu_state) menu_state->cleanup();
        delete menu_state;
        menu_state = nullptr;
        Log::log_info("Shut down main menu");
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
        playing_level = Levels::create_level();
    }
    else
    {
        Levels::destroy_level(playing_level);
        playing_level = nullptr;
        Log::log_info("Shut down playing level");
    }
}

void MenuState::step(float time_step)
{
    // Update each button's state
    v2 mouse_position = Platform::Input::mouse_world_position();
    for(Button *button : buttons)
    {
        v2 button_bl = button->position - button->full_extents * 0.5f;
        v2 button_tr = button->position + button->full_extents * 0.5f;
        
        // Check if player is hovering the button
        if(mouse_position.x >= button_bl.x &&
           mouse_position.x <= button_tr.x &&
           mouse_position.y >= button_bl.y &&
           mouse_position.y <= button_tr.y)
        {
            // If the mouse is clicked
            if(Platform::Input::mouse_button_down(0))
            {
                // Activate the button
                button->on_click();
            }

            // Highlight
            button->color.a = 1.0f;
        }
        else
        {
            // Un-highlight
            button->color.a = 0.5f;
        }
    }
}

void MenuState::draw()
{
    // Draw the menu
    // TODO: Separate UI and world space drawing
    Graphics::set_camera_position(v2());
    Graphics::set_camera_width(10.0f);
    for(Button *button : buttons)
    {
        Graphics::draw_quad(button->position, button->full_extents, 0.0f, button->color);
    }
}

void MenuState::cleanup()
{
    for(Button *button : buttons) delete button;
    buttons.clear();
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
    seconds_since_last_step = 0.0;
    last_loop_time = 0.0;

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

void Game::exit_to_main_menu()
{
    program_state->current_game_state->switch_game_mode(GameState::Mode::MAIN_MENU);
    program_state->switch_network_mode(ProgramState::NetworkMode::OFFLINE);
}

