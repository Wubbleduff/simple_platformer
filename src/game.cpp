
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

#include "imgui.h"



using namespace GameMath;

Engine *Engine::instance = nullptr;
const float Engine::TARGET_STEP_TIME = 0.01666f;
const int Timeline::MAX_STEPS_PER_UPDATE = 10;
static const int SERVER_PORT = 4242;
const float Engine::Client::TIMEOUT = 4.0f;



bool GameInput::action(Action action)
{
    return current_actions[(int)action];
}

void GameInput::serialize(Serialization::Stream *stream, bool serialize)
{
    if(serialize)
    {
        stream->write(uid);
        for(int i = 0; i < (int)Action::NUM_ACTIONS; i++)
        {
            stream->write((int)current_actions[i]);
        }
        stream->write(current_horizontal_movement);
        stream->write(current_aiming_direction);
    }
    else
    {
        stream->read(&uid);
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
void GameInput::read_from_local(v2 avatar_pos)
{
    current_actions[(int)Action::JUMP] = Platform::Input::key_down(' ');
    current_actions[(int)Action::SHOOT] = Platform::Input::mouse_button_down(0);

    float h = 0.0f;
    h -= Platform::Input::key('A') ? 1.0f : 0.0f;
    h += Platform::Input::key('D') ? 1.0f : 0.0f;
    current_horizontal_movement = h;

    if(GameMath::abs(current_horizontal_movement) > 1000.0f)
    {
        int b = 0;
        b++;
        GameConsole::write("Huge horizontal movement");
    }

    // TODO: This is spooky
    //v2 avatar_pos = Engine::instance->current_game_state->playing_level->get_avatar_position(uid);
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
        assert(GameMath::abs(current_horizontal_movement) < 1.5f);
        return true;
    }
    else
    {
        // Leave as defaults;
    }

    Serialization::free_stream(input_stream);

    return false;
}




void GameState::init()
{
    local_uid = -1;
    frame_number = 0;
    inputs_this_frame.clear();
}

void GameState::uninit()
{
}

void GameState::read_input()
{
}

void GameState::step(float time_step)
{
    frame_number++;
}

void GameState::draw()
{
}

void GameState::serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize)
{
}

#if DEBUG
void GameState::draw_debug_ui()
{
}
#endif



void GameStateMenu::init()
{
    GameState::init();

    mode = GameState::MAIN_MENU;

    screen = MAIN_MENU;
    confirming_quit_game = false;
    maybe_show_has_timed_out = false;
}

void GameStateMenu::uninit()
{
}

void GameStateMenu::read_input()
{
}

void GameStateMenu::step(float time_step)
{
    GameState::step(time_step);
}

void GameStateMenu::draw()
{
    switch(screen)
    {
        case MAIN_MENU:
            draw_main_menu();
            break;

        case JOIN_PLAYER:
            draw_join_player();
            break;
    }
}

void GameStateMenu::draw_main_menu()
{
    menu_window_begin();

    if(ImGui::Button("Start", button_size()))
    {
        Engine::switch_game_state(GameState::LOBBY);
    }

    if(ImGui::Button("Join other player", button_size()))
    {
        change_menu(JOIN_PLAYER);
    }

    for(int i = 0; i < 12; i++) ImGui::Spacing();

    if(confirming_quit_game)
    {
        ImGui::Text("Are you sure?");
        if(ImGui::Button("Quit", button_size()))
        {
            Engine::stop();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel", button_size()))
        {
            confirming_quit_game = false;
        }
    }
    else
    {
        if(ImGui::Button("Quit game", button_size()))
        {
            confirming_quit_game = true;
        }
    }

#if DEBUG
    for(int i = 0; i < 12; i++) ImGui::Spacing();
    if(ImGui::Button("Edit Game", button_size()))
    {
        Engine::switch_game_state(GameState::PLAYING_LEVEL);
        Engine::instance->editing = true;
    }
#endif

    ImGui::End();
}

void GameStateMenu::draw_join_player()
{
    menu_window_begin();

    static char address_string[16] = "127.0.0.1";
    ImGui::PushItemWidth(button_size().x);
    ImGui::InputText("IP Address", address_string, sizeof(address_string), 0);

    // Draw connecting buttons
    if(Engine::instance->client.connecting_timeout_timer > 0.0f)
    {
        if(ImGui::Button("Cancel", button_size()))
        {
            Engine::instance->client.connecting_timeout_timer = 0.0f;
            Engine::instance->client.disconnect_from_server();
            maybe_show_has_timed_out = false;
        }
        ImGui::SameLine();
        ImGui::Text("Connecting...");
    }
    else
    {
        if(ImGui::Button("Connect", button_size()))
        {
            Engine::connect(address_string, SERVER_PORT);
            maybe_show_has_timed_out = true;
        }
        else
        {
            if(maybe_show_has_timed_out)
            {
                ImGui::SameLine();
                ImGui::Text("Connection timed out!");
            }
        }
    }

    for(int i = 0; i < 12; i++) ImGui::Spacing();
    if(ImGui::Button("Main menu", button_size()))
    {
        change_menu(MAIN_MENU);
    }

    if(Platform::Input::key_down(Platform::Input::Key::ESC))
    {
        change_menu(MAIN_MENU);
    }

    menu_window_end();
}

void GameStateMenu::serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize)
{
}

#if DEBUG
void GameStateMenu::draw_debug_ui()
{
}
#endif

void GameStateMenu::change_menu(Screen in_screen)
{
    screen = in_screen;
}

void GameStateMenu::menu_window_begin()
{
    bool *p_open = NULL;
    ImGui::SetNextWindowPos( ImVec2(0, 0) );
    ImGui::SetNextWindowSize( ImVec2(Platform::Window::screen_width(), Platform::Window::screen_height()) );
    ImGui::Begin("Level select", p_open, IMGUI_MENU_WINDOW_FLAGS);
}
void GameStateMenu::menu_window_end()
{
    ImGui::End();
}
ImVec2 GameStateMenu::button_size()
{
    float button_scalar = Platform::Window::screen_width() * 0.2f;
    ImVec2 button_size = ImVec2(button_scalar, button_scalar * 0.25f);
    return button_size;
}



void GameStateLobby::init()
{
    mode = GameState::LOBBY;

    local_uid = 0;
    frame_number = 0;
    inputs_this_frame.clear();
    level = Levels::create_level(0);
}

void GameStateLobby::uninit()
{
    //Levels::destroy_level(playing_level);
    level = nullptr;
}

void GameState::read_input_for_level(Level *level)
{
    inputs_this_frame.clear();

    // Read input from local player
    GameInput local_input;
    // If we're a client, this field doesn't matter since we're sending
    // an input list to the server, it will just discard the uid
    local_input.uid = local_uid;
    v2 avatar_pos = v2();
    if(!level->avatars.empty() && level->avatars[local_uid] != nullptr)
    {
        avatar_pos = level->avatars[local_uid]->position;
    }
    local_input.read_from_local(avatar_pos);
    inputs_this_frame.push_back(local_input);

    // If we're a server, read inputs from all clients
    if(Engine::instance->network_mode == Engine::NetworkMode::SERVER)
    {
        // Read remote player's input
        for(Engine::Server::ClientConnection &client : Engine::instance->server.client_connections)
        {
            GameInput remote_input;
            remote_input.read_from_connection(&(client.connection));
            remote_input.uid = client.uid;
            if(client.connection != nullptr)
            {
                inputs_this_frame.push_back(remote_input);
            }
        }
        // Clean out disconnected connections
        Engine::instance->server.remove_disconnected_clients();
    }
}

void GameStateLobby::read_input()
{
    read_input_for_level(level);
}

void GameStateLobby::step(float time_step)
{
    GameState::step(time_step);
    level->step(inputs_this_frame, time_step);
}

void GameStateLobby::draw()
{
    level->draw(local_uid);

    if(Engine::instance->network_mode != Engine::NetworkMode::CLIENT)
    {
        ImGui::Begin("Select level");

        if(ImGui::Button("Level 1"))
        {
            Engine::switch_game_state(GameState::PLAYING_LEVEL);
            Levels::start_level(1);
        }
        if(ImGui::Button("Level 2"))
        {
            Engine::switch_game_state(GameState::PLAYING_LEVEL);
            Levels::start_level(2);
        }
        if(ImGui::Button("Open lobby"))
        {
            Engine::switch_network_mode(Engine::NetworkMode::SERVER);
        }
        if(ImGui::Button("Close lobby"))
        {
            Engine::switch_network_mode(Engine::NetworkMode::OFFLINE);
        }

        ImGui::End();
    }
}

void GameStateLobby::serialize(Serialization::Stream *stream, GameInput::UID uid_for_client, bool serialize)
{
    if(serialize)
    {
        stream->write(uid_for_client);
        stream->write(frame_number);
        stream->write((int)inputs_this_frame.size());
        for(GameInput &input : inputs_this_frame)
        {
            input.serialize(stream, true);
        }
        level->serialize(stream);
    }
    else
    {
        stream->read(&local_uid);
        stream->read(&frame_number);
        int num_inputs;
        stream->read(&num_inputs);
        inputs_this_frame.resize(num_inputs);
        for(int i = 0; i < num_inputs; i++)
        {
            GameInput *target = &(inputs_this_frame[i]);
            target->serialize(stream, false);
        }
        level->deserialize(stream);
    }
}

#if DEBUG
void GameStateLobby::draw_debug_ui()
{
    level->draw_debug_ui();
}
#endif



void GameStateLevel::init()
{
    mode = GameState::PLAYING_LEVEL;

    // Initialize state for playing level
    local_uid = -1;
    frame_number = 0;
    inputs_this_frame.clear();
    playing_level = Levels::active_level();
}

void GameStateLevel::uninit()
{
    //Levels::destroy_level(playing_level);
    playing_level = nullptr;

    Engine::instance->editing = false;
}

void GameStateLevel::read_input()
{
    read_input_for_level(playing_level);
}

void GameStateLevel::step(float time_step)
{
    GameState::step(time_step);


    if(Engine::instance->editing)
    {
        playing_level->editor.step(playing_level, time_step);
    }
    else
    {
        playing_level->step(inputs_this_frame, time_step);
    }

}

void GameStateLevel::draw()
{
    playing_level->draw(local_uid);

    if(Engine::instance->editing)
    {
        playing_level->editor.draw(playing_level);
    }
}

void GameStateLevel::serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize)
{
    if(serialize)
    {
        stream->write(uid);
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
        stream->read(&local_uid);
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

#if DEBUG
void GameStateLevel::draw_debug_ui()
{
    playing_level->draw_debug_ui();

}
#endif












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
    Engine::switch_game_state(GameState::MAIN_MENU);
}

bool Engine::Client::is_connected()
{
    if(server_connection == nullptr) return false;
    return server_connection->is_connected();
}

void Engine::Client::update_connection(float time_step)
{
    if(server_connection == nullptr) return;

    server_connection->check_on_connection_status();

    if(server_connection->is_connected())
    {
        connecting_timeout_timer = 0.0f;
        return;
    }

    //Log::log_info("Connecting...");
    connecting_timeout_timer += time_step;
    if(connecting_timeout_timer >= TIMEOUT)
    {
        //Log::log_info("Timeout connecting to server!");
        disconnect_from_server();
        connecting_timeout_timer = 0.0f;
        return;
    }
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
    Platform::Input::read_input();
    game_state->read_input();

    game_state->step(time_step);

    // Do drawing
    {

        Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
        Graphics::ImGuiImplementation::new_frame();
        game_state->draw();
        Graphics::render();
        draw_debug_menu();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void Engine::step_as_client(GameState *game_state, float time_step)
{ 
    Engine::instance->client.update_connection(time_step);

    bool game_state_valid = true;

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

            // Check if the server's game state has changed
            // If so, we should too
            GameState::Mode server_mode;
            game_stream->read((int *)&server_mode);
            if(server_mode != game_state->mode)
            {
                Engine::switch_game_state(server_mode);
                // Drop the first read game state on the floor
                game_state_valid = false;
            }

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
            if(game_state_valid)
            {
                game_state->serialize(game_stream, -1, false);
            }
        }

        Serialization::free_stream(game_stream);
    }
    else
    {
        game_state->step(time_step);
    }

    if(game_state_valid)
    {
        Platform::Input::read_input();

        // Send the local player's input to the server
        if(Engine::instance->client.is_connected())
        {
            Serialization::Stream *input_stream = Serialization::make_stream();

            GameInput local_input;
            local_input.read_from_local(v2()); // TODO: Fix this!
            assert(GameMath::abs(local_input.current_horizontal_movement) < 1.5f);
            local_input.serialize(input_stream, true);

            Engine::instance->client.server_connection->send_stream(input_stream);
            Serialization::free_stream(input_stream);
        }


#define CLIENT_PREDICTION 0
#if CLIENT_PREDICTION
        // Step the game forward in time
        // ...
#endif

// Draw
        {
            Graphics::clear_frame(v4(0.0f, 0.5f, 0.75f, 1.0f) * 0.1f);
            Graphics::ImGuiImplementation::new_frame();

            game_state_valid = instance->client.is_connected();
            if(game_state_valid)
            {
                game_state->draw();
            }

            draw_debug_menu();

            Graphics::render();

            Graphics::ImGuiImplementation::end_frame();
            Graphics::swap_frames();
        }
    }
}

// TODO: Pass the current game state and server state here
void Engine::step_as_server(GameState *game_state, float time_step)
{
    // Accept new connections to the server
    std::vector<Network::Connection *> new_connections = Network::accept_client_connections();
    for(int i = 0; i < new_connections.size(); i++)
    {
        Network::Connection *new_connection = new_connections[i];
        Engine::instance->server.add_client_connection(new_connection);
    }

    // Prepare for reading input
    Platform::Input::read_input();
    game_state->read_input();

    // Step the game state using all inputs
    game_state->step(time_step);

    // Broadcast the players and game state
    {
        Serialization::Stream *game_stream = Serialization::make_stream();
        for(Engine::Server::ClientConnection &client : Engine::instance->server.client_connections)
        {
            game_stream->write((int)game_state->mode);
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
        draw_debug_menu();
        Graphics::ImGuiImplementation::end_frame();
        Graphics::swap_frames();
    }
}

void Engine::draw_debug_menu()
{
#if DEBUG
    {
        ImGui::Begin("Debug");

        if(ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
        {
            if(ImGui::BeginTabItem("Networking"))
            {

                if(Engine::instance->network_mode == NetworkMode::OFFLINE)
                {
                    ImGui::Text("OFFLINE");
                    if(ImGui::Button("Switch to server")) Engine::switch_network_mode(Engine::NetworkMode::SERVER);
                    if(ImGui::Button("Switch to client"))  Engine::switch_network_mode(Engine::NetworkMode::CLIENT);
                    ImGui::EndTabItem();
                }
                else if(Engine::instance->network_mode == NetworkMode::SERVER)
                {
                    ImGui::Text("SERVER");
                    if(ImGui::Button("Switch to offline")) Engine::switch_network_mode(Engine::NetworkMode::OFFLINE);
                    if(ImGui::Button("Switch to client"))  Engine::switch_network_mode(Engine::NetworkMode::CLIENT);
                    ImGui::EndTabItem();
                }
                else if(Engine::instance->network_mode == NetworkMode::CLIENT)
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

            ImGui::EndTabBar();
        }

        ImGui::End(); // Debug
    }
#endif
}

void Engine::do_one_step(float time_step)
{
    GameState *current_game_state = instance->current_game_state;

    if(instance->next_mode != instance->current_mode)
    {
        current_game_state->uninit();
        delete current_game_state;

        switch(instance->next_mode)
        {
        case GameState::Mode::MAIN_MENU:
            instance->switch_network_mode(NetworkMode::OFFLINE);
            instance->current_game_state = new GameStateMenu();
            break;
        case GameState::Mode::LOBBY:
            instance->current_game_state = new GameStateLobby();
            break;
        case GameState::Mode::PLAYING_LEVEL:
            instance->current_game_state = new GameStateLevel();
            break;
        default:
            instance->current_game_state = nullptr;
        }
        current_game_state = instance->current_game_state;

        current_game_state->init();
        instance->current_mode = instance->next_mode;
        assert(instance->current_mode == current_game_state->mode);
    }

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
    instance->current_game_state = new GameStateMenu();
    instance->current_game_state->init();

    instance->timeline->reset();
    instance->timeline->step_with_frequency(Engine::TARGET_STEP_TIME);
}

void Engine::switch_game_state(GameState::Mode mode)
{
    instance->next_mode = mode;
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
    Levels::init();

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
