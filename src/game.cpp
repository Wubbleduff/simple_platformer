
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
    enum Screen
    {
        MAIN_MENU,
        LEVEL_SELECT
    };

    Screen screen = MAIN_MENU;

    void change_screen(Screen screen);
    void step(float time_step);
    void draw();
    void draw_main_menu();
    void draw_level_select();
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
    int level_to_start = 0;
    Level *playing_level;  // For when the player is playing the game

    void read_input_as_offline();
    void read_input_as_client();
    void read_input_as_server();
    void step(GameInput::UID focus_uid, float time_step);
    void draw();
    void switch_game_mode(GameState::Mode mode);
    void check_for_mode_switch();
    void init_for_main_menu(bool initting);
    void init_for_playing_level(bool initting);
    void start_level(int level);
    void serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize);

#if DEBUG
    void draw_debug_ui();
#endif
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

// Engine processes game states
struct Engine
{
    static Engine *instance;

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



    static void start();
    static void stop();
    static void init();
    
    // Switches to client mode and connects to a server
    static void connect(const char *ip_address, int port);

    // Switch to a network mode (local, client, server)
    static void switch_network_mode(NetworkMode mode);

    // Engine tick
    static void do_one_step(float time_step);
    static void step_as_offline(GameState *game_state, float time_step);
    static void step_as_client(GameState *game_state, float time_step);
    static void step_as_server(GameState *game_state, float time_step);

    static void shutdown();
};
Engine *Engine::instance = nullptr;

const float Engine::TARGET_STEP_TIME = 0.01666f;
static const int SERVER_PORT = 4242;
const float Engine::Client::TIMEOUT = 2.0f;



bool GameInput::action(Action action)
{
    return current_actions[(int)action];
}

void GameInput::serialize(Serialization::Stream *stream, bool serialize)
{
    if(serialize)
    {
        for(int i = 0; i < (int)Action::NUM_ACTIONS; i++)
        {
            stream->write((int)current_actions[i]);
        }

        stream->write(current_horizontal_movement);
        stream->write(current_aiming_direction);
    }
    else
    {
        for(int i = 0; i < (int)Action::NUM_ACTIONS; i++)
        {
            int value;
            stream->read(&value);
            current_actions[i] = (value == 0) ? false : true;
        }

        stream->read(&current_horizontal_movement);
        stream->read(&current_aiming_direction);
    }
    
}

// TODO: Should I pass current_game_state here?
void GameInput::read_from_local()
{
    current_actions[(int)Action::JUMP] = Platform::Input::key_down(' ');
    current_actions[(int)Action::SHOOT] = Platform::Input::mouse_button_down(0);

    float h = 0.0f;
    h -= Platform::Input::key('A') ? 1.0f : 0.0f;
    h += Platform::Input::key('D') ? 1.0f : 0.0f;
    current_horizontal_movement = h;

    // TODO: This is spooky
    v2 avatar_pos = Engine::instance->current_game_state->playing_level->get_avatar_position(uid);
    current_aiming_direction = Platform::Input::mouse_world_position() - avatar_pos;
}

bool GameInput::read_from_connection(Network::Connection **connection)
{
    assert(*connection != nullptr);

    Serialization::Stream *input_stream = Serialization::make_stream();

    Network::ReadResult result = (*connection)->read_into_stream(input_stream);

    if(result == Network::ReadResult::CLOSED)
    {
        Network::disconnect(connection);
        Serialization::free_stream(input_stream);
        return false;
    }
    else if(result == Network::ReadResult::READY)
    {
        input_stream->move_to_beginning();

        // TODO: Sanitize ...

        serialize(input_stream, false);
        return true;
    }

    Serialization::free_stream(input_stream);

    return false;
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

void GameState::read_input_as_offline()
{
    inputs_this_frame.clear();

    switch(current_mode)
    {
        case Mode::MAIN_MENU:
            // ...
            break;

        case Mode::PLAYING_LEVEL:
        {
            // Read input from local player
            GameInput local_input;
            local_input.uid = 0;
            local_input.read_from_local();
            inputs_this_frame.push_back(local_input);
        }
        break;

        case Mode::EDITING:
            // ...
            break;
    }

}

void GameState::read_input_as_client()
{
    inputs_this_frame.clear();

    switch(current_mode)
    {
        case Mode::MAIN_MENU:
            // ...
            break;

        case Mode::PLAYING_LEVEL:
        {
            // Read input from local player
            GameInput local_input;
            //local_input.uid = 0; This field doesn't matter since we're sending an input list to the server, it will just discard the uid
            local_input.read_from_local();
            inputs_this_frame.push_back(local_input);
        }
        break;

        case Mode::EDITING:
            // ...
            break;
    }
}

void GameState::read_input_as_server()
{
    inputs_this_frame.clear();

    switch(current_mode)
    {
        case Mode::MAIN_MENU:
            // ...
            break;
        case Mode::PLAYING_LEVEL:
        {
            // Read local player's input
            GameInput local_input;
            local_input.uid = 0;
            local_input.read_from_local();
            inputs_this_frame.push_back(local_input);

            // Read remote player's input
            for(Engine::Server::ClientConnection &client : Engine::instance->server.client_connections)
            {
                GameInput remote_input;
                remote_input.uid = client.uid;
                remote_input.read_from_connection(&(client.connection));
                if(client.connection != nullptr)
                {
                    inputs_this_frame.push_back(remote_input);
                }
            }
            // Clean out disconnected connections
            Engine::instance->server.remove_disconnected_clients();

        }
        break;

        case Mode::EDITING:
            // ...
            break;
    }
}

void GameState::step(GameInput::UID focus_uid, float time_step)
{
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
        //playing_level->edit_draw();
        break;
    }
}

void GameState::serialize(Serialization::Stream *stream, GameInput::UID other_uid, bool serialize)
{
    if(serialize)
    {
        assert(current_mode == PLAYING_LEVEL);

        stream->write(other_uid);
        stream->write(frame_number);
        stream->write((int)inputs_this_frame.size());
        for(GameInput &input : inputs_this_frame)
        {
            input.serialize(stream, true);
        }
        playing_level->serialize(stream);
    }
    else
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
            target->serialize(stream, false);
        }
        playing_level->deserialize(stream);
    }
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
        playing_level = create_level(level_to_start);
    }
    else
    {
        destroy_level(playing_level);
        playing_level = nullptr;
    }
}

void GameState::start_level(int level)
{
    level_to_start = level;
    switch_game_mode(Mode::PLAYING_LEVEL);
}

#if DEBUG
void GameState::draw_debug_ui()
{
    switch(current_mode)
    {
    case Mode::MAIN_MENU:
        break;
    case Mode::PLAYING_LEVEL:
        playing_level->draw_debug_ui();
        break;
    case Mode::EDITING:
        playing_level->edit_draw();
        break;
    }
}
#endif

void MenuState::change_screen(MenuState::Screen in_screen)
{
    screen = in_screen;
}

void MenuState::step(float time_step)
{
}

// TODO: Should I pass current_game_state here?
void MenuState::draw()
{
    switch(screen)
    {
        case MAIN_MENU:
            draw_main_menu();
            break;

        case LEVEL_SELECT:
            draw_level_select();
            break;
    }
}

void MenuState::draw_main_menu()
{
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    //window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoNav;
    //window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    bool *p_open = NULL;

    ImGui::SetNextWindowPos( ImVec2(0, 0) );
    ImGui::SetNextWindowSize( ImVec2(Platform::Window::screen_width(), Platform::Window::screen_height()) );
    ImGui::Begin("Main Menu", p_open, window_flags);
    float button_scalar = Platform::Window::screen_width() * 0.2f;
    ImVec2 button_size = ImVec2(button_scalar, button_scalar * 0.25f);
    if(ImGui::Button("Start", button_size))
    {
        change_screen(LEVEL_SELECT);
    }
    static char address_string[16] = "127.0.0.1";
    if(ImGui::Button("Join player", button_size))
    {
        Engine::connect(address_string, SERVER_PORT);
    }
    ImGui::SameLine();
    ImGui::InputText("Address", address_string, sizeof(address_string), 0);

    if(ImGui::Button("Open lobby", button_size))
    {
        Engine::switch_network_mode(Engine::NetworkMode::SERVER);
    }

    for(int i = 0; i < 12; i++) ImGui::Spacing();
    if(ImGui::Button("Quit game", button_size))
    {
        Engine::stop();
    }


#if DEBUG
    for(int i = 0; i < 12; i++) ImGui::Spacing();
    if(ImGui::Button("Edit Game", button_size))
    {
        // TODO: Here
        // Do something like Engine::start_editing()
        Engine::instance->current_game_state->switch_game_mode(GameState::Mode::EDITING);
    }
#endif

    ImGui::End();
}

void MenuState::draw_level_select()
{
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    //window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoNav;
    //window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    bool *p_open = NULL;

    ImGui::SetNextWindowPos( ImVec2(0, 0) );
    ImGui::SetNextWindowSize( ImVec2(Platform::Window::screen_width(), Platform::Window::screen_height()) );
    ImGui::Begin("Level select", p_open, window_flags);
    float button_scalar = Platform::Window::screen_width() * 0.2f;
    ImVec2 button_size = ImVec2(button_scalar, button_scalar * 0.25f);
    if(ImGui::Button("Level 1", button_size))
    {
        Engine::instance->current_game_state->start_level(0);
    }
    if(ImGui::Button("Level 2", button_size))
    {
        Engine::instance->current_game_state->start_level(1);
    }

    for(int i = 0; i < 12; i++) ImGui::Spacing();
    if(ImGui::Button("Main menu", button_size))
    {
        change_screen(MAIN_MENU);
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
                Engine::do_one_step(step_frequency);
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
                Engine::do_one_step(step_frequency);
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




void Engine::Client::disconnect_from_server()
{
    Network::disconnect(&server_connection);

    // Reset the level's state
    instance->current_game_state->switch_game_mode(GameState::Mode::MAIN_MENU);
}

bool Engine::Client::is_connected()
{
    if(server_connection == nullptr) return false;
    return server_connection->is_connected();
}

void Engine::Client::update_connection(float time_step)
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




bool Engine::Server::startup(int port)
{
    bool success = Network::listen_for_client_connections(port);
    return success;
}

void Engine::Server::shutdown()
{
    for(ClientConnection &client : client_connections)
    {
        Network::disconnect(&(client.connection));
    }
    client_connections.clear();
    Network::stop_listening_for_client_connections();
    Log::log_info("Server shutdown");
}

void Engine::Server::add_client_connection(Network::Connection *connection)
{
    client_connections.push_back( {next_input_uid, connection} );
    next_input_uid++;
}

void Engine::Server::remove_disconnected_clients()
{
    auto it = std::remove_if(client_connections.begin(), client_connections.end(),
            [](const ClientConnection &client) { return client.connection == nullptr; }
            );
    client_connections.erase(it, client_connections.end());
}




void Engine::step_as_offline(GameState *game_state, float time_step)
{
    game_state->check_for_mode_switch();

    Platform::Input::read_input();
    game_state->read_input_as_offline();

    game_state->step(0, time_step);

    // Do drawing
    {

        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        game_state->draw();

        Graphics::render();

#if DEBUG
        {
            ImGui::Begin("Debug");

            if(ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
            {
                if(ImGui::BeginTabItem("Networking"))
                {
                    ImGui::Text("OFFLINE");
                    if(ImGui::Button("Switch to client")) Engine::switch_network_mode(Engine::NetworkMode::CLIENT);
                    if(ImGui::Button("Switch to server")) Engine::switch_network_mode(Engine::NetworkMode::SERVER);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Console"))
                {
                    GameConsole::draw();
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Frame times"))
                {
                    float avg = GameMath::average(Engine::instance->timeline->step_times.data(), Engine::instance->timeline->step_times.size());
                    ImGui::Text("Average step time: %f", avg);
                    ImGui::PlotHistogram("Step Times", Engine::instance->timeline->step_times.data(), Engine::instance->timeline->step_times.size(), 0, 0, 0.0f, 0.016f, ImVec2(512.0f, 256.0f));
                    ImGui::EndTabItem();
                }

                game_state->draw_debug_ui();

                ImGui::EndTabBar();
            }

            ImGui::End(); // Debug
        }
#endif


        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

// TODO: Pass the current game state and the client state here
void Engine::step_as_client(GameState *game_state, float time_step)
{ 
    game_state->check_for_mode_switch();

    Engine::instance->client.update_connection(time_step);

    // TODO:
    // Once doing client side prediction, read input using the game state's read_input
    // like other program states are doing

    
    Platform::Input::read_input();


    // Send the local player's input to the server
    if(Engine::instance->client.is_connected())
    {
        Serialization::Stream *input_stream = Serialization::make_stream();

        game_state->read_input_as_client();

        for(GameInput &input : game_state->inputs_this_frame)
        {
            input.serialize(input_stream, true);
        }

        Engine::instance->client.server_connection->send_stream(input_stream);
        Serialization::free_stream(input_stream);
    }

    

#define CLIENT_PREDICTION 0
#if CLIENT_PREDICTION
    // Step the game forward in time
    // ...
#endif

    

    // Read server's game state
    if(Engine::instance->client.is_connected())
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        Network::ReadResult result = Engine::instance->client.server_connection->read_into_stream(game_stream);

        if(result == Network::ReadResult::CLOSED)
        {
            Engine::instance->client.disconnect_from_server();
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
            // Pass -1 for now  because it doesn't make sense in this context (2/6/2021)
            game_state->serialize(game_stream, -1, false);
        }

        Serialization::free_stream(game_stream);
    }
    else
    {
        unsigned int focus = 0;
        game_state->step(focus, time_step);
    }

    // Draw
    {
        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();


        bool game_state_still_valid = true;

#if DEBUG
        {
            ImGui::Begin("Debug");

            if(ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
            {
                if(ImGui::BeginTabItem("Networking"))
                {
                    ImGui::Text("CLIENT");

                    if(ImGui::Button("Switch to offline")) Engine::switch_network_mode(Engine::NetworkMode::OFFLINE);
                    if(ImGui::Button("Switch to server"))  Engine::switch_network_mode(Engine::NetworkMode::SERVER);

                    
                    static char address_string[16] = "127.0.0.1";
                    ImGui::InputText("Address", address_string, sizeof(address_string), 0);
                    if(Engine::instance->client.is_connected())
                    {
                        if(ImGui::Button("Disconnect from server"))
                        {
                            Engine::instance->client.disconnect_from_server();
                            game_state_still_valid = false;
                        }
                    }
                    else
                    {
                        if(ImGui::Button("Connect to server"))
                        {
                            Engine::connect(address_string, SERVER_PORT);
                        }
                    }

                    ImGui::EndTabItem(); // Networking
                }
                if(ImGui::BeginTabItem("Console"))
                {
                    GameConsole::draw();
                    ImGui::EndTabItem(); // Console
                }
                if(ImGui::BeginTabItem("Frame times"))
                {
                    ImGui::EndTabItem(); // Frame times
                }

                game_state->draw_debug_ui();

                ImGui::EndTabBar(); // ##tabs
            }

            ImGui::End(); // Debug
        }
#endif


        if(game_state_still_valid)
        {
            game_state->draw();
        }

        Graphics::render();

        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

// TODO: Pass the current game state and server state here
void Engine::step_as_server(GameState *game_state, float time_step)
{
    game_state->check_for_mode_switch();

    // Accept new connections to the server
    std::vector<Network::Connection *> new_connections = Network::accept_client_connections();
    for(int i = 0; i < new_connections.size(); i++)
    {
        Network::Connection *new_connection = new_connections[i];
        Engine::instance->server.add_client_connection(new_connection);
    }

    // Prepare for reading input
    Platform::Input::read_input();
    game_state->read_input_as_server();

    // Step the game state using all inputs
    game_state->step(0, time_step);

    // Broadcast the players and game state
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        for(Engine::Server::ClientConnection &client : Engine::instance->server.client_connections)
        {
            game_state->serialize(game_stream, client.uid, true);
            client.connection->send_stream(game_stream);
            game_stream->clear();
        }
        Serialization::free_stream(game_stream);
    }



    // Draw game
    {
        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        game_state->draw();

        Graphics::render();

#if DEBUG



        {
            ImGui::Begin("Debug");

            if(ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
            {
                if(ImGui::BeginTabItem("Networking"))
                {
                    if(ImGui::Button("Switch to offline")) Engine::switch_network_mode(Engine::NetworkMode::OFFLINE);
                    if(ImGui::Button("Switch to client"))  Engine::switch_network_mode(Engine::NetworkMode::CLIENT);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Console"))
                {
                    GameConsole::draw();
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Frame times"))
                {
                    ImGui::EndTabItem();
                }

                game_state->draw_debug_ui();

                ImGui::EndTabBar();
            }

            ImGui::End(); // Debug
        }
#endif

        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void Engine::do_one_step(float time_step)
{
    GameState *current_game_state = instance->current_game_state;

    switch(instance->network_mode)
    {
        case NetworkMode::OFFLINE:
        {
            step_as_offline(current_game_state, time_step);
            break;
        }
        case NetworkMode::CLIENT:
        {
            step_as_client(current_game_state, time_step);
            break;
        }
        case NetworkMode::SERVER:
        {
            step_as_server(current_game_state, time_step);
            break;
        }
    }
}

void Engine::switch_network_mode(NetworkMode mode)
{
    if(mode == NetworkMode::OFFLINE)
    {
        if(instance->network_mode == NetworkMode::CLIENT)
        {
            instance->client.disconnect_from_server();
        }
        if(instance->network_mode == NetworkMode::SERVER)
        {
            instance->server.shutdown();
        }
        instance->network_mode = NetworkMode::OFFLINE;
    }

    if(mode == NetworkMode::CLIENT)
    {
        if(instance->network_mode == NetworkMode::SERVER)
        {
            instance->server.shutdown();
        }
        instance->network_mode = NetworkMode::CLIENT;
        instance->client.server_connection = nullptr;
    }

    if(mode == NetworkMode::SERVER)
    {
        if(instance->network_mode == NetworkMode::CLIENT)
        {
            instance->client.disconnect_from_server();
        }
        bool success = instance->server.startup(SERVER_PORT);
        if(success)
        {
            instance->network_mode = NetworkMode::SERVER;
        }
    }
}

void Engine::connect(const char *ip_address, int port)
{
    Engine::switch_network_mode(Engine::NetworkMode::CLIENT);
    instance->client.server_connection = Network::connect(ip_address, port);
}

void Engine::init()
{
    instance = new Engine();

    instance->running = true;
    instance->timeline = new Timeline();

    instance->network_mode = Engine::NetworkMode::OFFLINE;
    instance->current_game_state = new GameState();
    instance->current_game_state->current_mode = GameState::Mode::INVALID;
    instance->current_game_state->switch_game_mode(GameState::Mode::MAIN_MENU);

    instance->timeline->reset();
    instance->timeline->step_with_frequency(Engine::TARGET_STEP_TIME);
}

void Engine::shutdown()
{
    switch(instance->network_mode)
    {
        case NetworkMode::OFFLINE:
        {
            break;
        }

        case NetworkMode::CLIENT:
        {
            instance->client.disconnect_from_server();
            break;
        }

        case NetworkMode::SERVER:
        {
            instance->server.shutdown();
            break;
        }
    }

    Graphics::ImGuiImplementation::shutdown();
}



void Engine::start()
{
    Platform::init();
    Log::init();
    GameConsole::init();
    Graphics::init();
    Network::init();

    //seed_random(0);
    seed_random((int)(Platform::time_since_start() * 10000.0f));

    init();

    while(instance->running)
    {
        // Check on OS events as frequently as possible
        Platform::handle_os_events();
        if(instance->running == false) break;

        instance->timeline->update();
    }

    shutdown();
}

void Engine::stop()
{
    instance->running = false;
}








// TODO: Remove
GameInput::UID Game::get_my_uid()
{
    return Engine::instance->current_game_state->my_uid;
}
void Game::start() { Engine::start(); }
void Game::stop() { Engine::stop(); }

// TODO: Make this like "CurrentGameState::exit_to_main_menu()" or something like that
void Game::exit_to_main_menu()
{
    Engine::instance->current_game_state->switch_game_mode(GameState::Mode::MAIN_MENU);
    Engine::switch_network_mode(Engine::NetworkMode::OFFLINE);
}


