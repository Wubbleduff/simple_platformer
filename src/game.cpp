
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



#if 1

void MenuInput::read_from_local()
{
}

void MenuInput::serialize(Serialization::Stream *stream, bool serialize)
{
}

void AvatarInput::serialize(Serialization::Stream *stream, bool serialize)
{
    if(serialize)
    {
        stream->write(uid);
        stream->write(horizontal_movement);
        stream->write(jump == false ? 0 : 1);
        stream->write(aiming_direction);
        stream->write(shoot == false ? 0 : 1);
    }
    else
    {
        stream->read(&uid);
        stream->read(&horizontal_movement);
        stream->read((int *)&jump);
        stream->read(&aiming_direction);
        stream->read((int *)&shoot);
    }
    
}

// TODO: Should I pass current_game_state here?
void AvatarInput::read_from_local(Avatar *avatar)
{
    jump = Platform::Input::key_down(' ');
    shoot = Platform::Input::mouse_button_down(0);

    float h = 0.0f;
    h -= Platform::Input::key('A') ? 1.0f : 0.0f;
    h += Platform::Input::key('D') ? 1.0f : 0.0f;
    horizontal_movement = h;

    if(avatar)
    {
        aiming_direction = Platform::Input::mouse_world_position() - avatar->position;
    }
    else
    {
        aiming_direction = v2();
    }   
}

bool AvatarInput::read_from_connection(Network::Connection **connection)
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
#endif



Avatar *AvatarList::add(UID uid, Level *level)
{
    std::map<UID, Avatar *>::iterator it = avatar_map.find(uid);
    assert(it == avatar_map.end());

    Avatar *avatar = new Avatar();
    avatar_map[uid] = avatar;

    avatar->reset(level);

    return avatar;
}

Avatar *AvatarList::find(UID uid)
{
    std::map<UID, Avatar *>::iterator it = avatar_map.find(uid);
    if(it == avatar_map.end())
    {
        return nullptr;
    }
    return it->second;
}

void AvatarList::remove(UID uid)
{
    avatar_map.erase(uid);
}

void AvatarList::sync_with_inputs(AvatarInputList inputs, Level *level)
{
    // Sync and match avatars to inputs
    // Each input should map to one avatar to control
    // TODO: This should be done differently
    for(AvatarInput &input : inputs)
    {
        auto it = avatar_map.find(input.uid);
        if(it == avatar_map.end())
        {
            add(input.uid, level);
        }
    }
    std::vector<UID> uids_to_remove;
    for(const std::pair<UID, Avatar *> &pair : avatar_map)
    {
        auto it = std::find_if(inputs.begin(), inputs.end(),
            [&](const AvatarInput &input) { return input.uid == pair.first; });
        if(it == inputs.end())
        {
            uids_to_remove.push_back(pair.first);
        }
    }
    while(uids_to_remove.size() > 0)
    {
        remove(uids_to_remove.back());
        uids_to_remove.pop_back();
    }
}

void AvatarList::step(AvatarInputList inputs, Level *level, float time_step)
{
    sync_with_inputs(inputs, level);

    for(AvatarInput &input : inputs)
    {
        Avatar *avatar = find(input.uid);
        avatar->step(&input, level, time_step);
    }
}

void AvatarList::draw()
{
    for(std::pair<UID, Avatar *> pair : avatar_map)
    {
        Avatar *avatar = pair.second;
        avatar->draw();
    }
}

#pragma region Avatar

void Avatar::reset(Level *level)
{
    position = level->grid.cell_to_world(level->grid.start_cell);

    grounded = false;
    horizontal_velocity = 0.0f;
    vertical_velocity = 0.0f;
    run_strength = 256.0f;
    friction_strength = 16.0f;
    mass = 4.0f;
    gravity = 9.81f;
    full_extent = 1.0f;
    color = random_color();
}

void Avatar::step(AvatarInput *input, Level *level, float time_step)
{
    float horizontal_acceleration = 0.0f;
    float vertical_acceleration = 0.0f;

    bool jump = input->jump;
    bool shoot = input->shoot;

    horizontal_acceleration += input->horizontal_movement * run_strength;

    if(grounded)
    {
        vertical_velocity = 0.0f;

        if(jump)
        {
            vertical_velocity = 20.0f;
            grounded = false;
        }
    }
    else
    {
        //vertical_acceleration -= gravity * mass;
    }

    if(shoot)
    {
        Log::log_info("Bang!");
    }

    vertical_acceleration -= gravity * mass;
    horizontal_acceleration -= horizontal_velocity * friction_strength;

    horizontal_velocity += horizontal_acceleration * time_step;
    vertical_velocity += vertical_acceleration * time_step;
    position.x += horizontal_velocity * time_step;
    position.y += vertical_velocity * time_step;

    check_and_resolve_collisions(level);

    if(position.y < -5.0f)
    {
        //level->change_mode(LOSS);
    }
}

void Avatar::draw()
{
    Graphics::quad(position, v2(1.0f, 1.0f) * full_extent, 0.0f, color);
}

static bool aabb(v2 a_bl, v2 a_tr, v2 b_bl, v2 b_tr, v2 *dir, float *depth)
{
    float a_left   = a_bl.x;
    float a_right  = a_tr.x;
    float a_top    = a_tr.y;
    float a_bottom = a_bl.y;

    float b_left   = b_bl.x;
    float b_right  = b_tr.x;
    float b_top    = b_tr.y;
    float b_bottom = b_bl.y;

    float left_diff   = a_left - b_right;
    float right_diff  = b_left - a_right;
    float top_diff    = b_bottom - a_top;
    float bottom_diff = a_bottom - b_top;

    float max_depth = max(left_diff, max(right_diff, max(top_diff, bottom_diff)));

    if(max_depth > 0.0f)
    {
        return false;
    }

    if(max_depth == left_diff)   { *dir = v2(-1.0f, 0.0f); }
    if(max_depth == right_diff)  { *dir = v2(1.0f, 0.0f); }
    if(max_depth == top_diff)    { *dir = v2(0.0f, 1.0f); }
    if(max_depth == bottom_diff) { *dir = v2(0.0f, -1.0f); }

    *depth = -max_depth;

    return true;
}
void Avatar::check_and_resolve_collisions(Level *level)
{
    v2 avatar_bl = position - v2(1.0f, 1.0f) * full_extent * 0.5f;
    v2 avatar_tr = position + v2(1.0f, 1.0f) * full_extent * 0.5f;

    std::vector<float> rights;
    std::vector<float> lefts;
    std::vector<float> ups;
    std::vector<float> downs;
    bool won_level = false;

    v2i bl = level->grid.world_to_cell(avatar_bl) - v2i(1, 1);
    v2i tr = level->grid.world_to_cell(avatar_tr) + v2i(1, 1);
    for(v2i pos = bl; pos.y <= tr.y; pos.y++)
    {
        for(pos.x = bl.x; pos.x <= tr.x; pos.x++)
        {
            if(level->grid.at(pos) == nullptr) continue;

            if(level->grid.at(pos)->filled)
            {
                v2 cell_world_position = level->grid.cell_to_world(pos);
                v2 cell_bl = cell_world_position;
                v2 cell_tr = cell_world_position + v2(1.0f, 1.0f) * level->grid.world_scale;
                v2 dir;
                float depth;
                bool collision = aabb(cell_bl, cell_tr, avatar_bl, avatar_tr, &dir, &depth);
                if(collision)
                {
                    bool blocked = false;
                    v2 n_dir = normalize(dir);
                    v2i dir_i = v2i((int)(n_dir.x), (int)(n_dir.y));
                    v2i pos_in_question = pos + dir_i;
                    if(level->grid.at(pos_in_question) && level->grid.at(pos_in_question)->filled)
                    {
                        blocked = true;
                    }

                    if(!blocked)
                    {
                        if(dir_i.x ==  1 && dir_i.y ==  0) rights.push_back(depth);
                        if(dir_i.x == -1 && dir_i.y ==  0) lefts.push_back(depth);
                        if(dir_i.x ==  0 && dir_i.y ==  1) ups.push_back(depth);
                        if(dir_i.x ==  0 && dir_i.y == -1) downs.push_back(depth);
                    }
                }
            }

            if(level->grid.at(pos)->win_when_touched)
            {
                v2 cell_world_position = level->grid.cell_to_world(pos);
                v2 cell_bl = cell_world_position;
                v2 cell_tr = cell_world_position + v2(1.0f, 1.0f) * level->grid.world_scale;
                v2 dir;
                float depth;
                bool collision = aabb(cell_bl, cell_tr, avatar_bl, avatar_tr, &dir, &depth);
                if(collision)
                {
                    won_level = true;
                }
            }
        }
    }

    v2 resolution = v2();
    float max_right = 0.0f;
    float max_left = 0.0f;
    float max_up = 0.0f;
    float max_down = 0.0f;
    for(int i = 0; i < rights.size(); i++) { max_right = max(rights[i], max_right); }
    for(int i = 0; i < lefts.size(); i++)  { max_left  = max(lefts[i], max_left); }
    for(int i = 0; i < ups.size(); i++)    { max_up    = max(ups[i], max_up); }
    for(int i = 0; i < downs.size(); i++)  { max_down  = max(downs[i], max_down); }
    resolution += v2(1.0f, 0.0f) * max_right;
    resolution += v2(-1.0f, 0.0f) * max_left;
    resolution += v2(0.0f, 1.0f) * max_up;
    resolution += v2(0.0f, -1.0f) * max_down;

    position += resolution;

    if(length_squared(resolution) == 0.0f)
    {
        grounded = false;
    }
    if(resolution.y > 0.0f && vertical_velocity < 0.0f)
    {
        grounded = true;
    }
    if(resolution.y < 0.0f && vertical_velocity > 0.0f)
    {
        vertical_velocity = 0.0f;
    }
    if(GameMath::abs(resolution.x) > 0.0f)
    {
        horizontal_velocity = 0.0f;
    }

    if(won_level)
    {
        //level->change_mode(Level::Mode::WIN);
    }
}





// HERE
#if 0
void Level::serialize(Serialization::Stream *stream)
{
    stream->write((int)avatars.size());
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        GameInput::UID uid = pair.first;
        Avatar *avatar = pair.second;

        stream->write(uid);
        stream->write(avatar->position);
        stream->write(avatar->color);
    }
}

void Level::deserialize(Serialization::Stream *stream)
{
    int num_avatars;
    stream->read(&num_avatars);
    std::vector<GameInput::UID> uids_seen;
    for(int i = 0; i < num_avatars; i++)
    {
        GameInput::UID uid;
        stream->read((int *)&uid);
        uids_seen.push_back(uid);

        Avatar *avatar = nullptr;
        auto it = avatars.find(uid);
        if(it == avatars.end())
        {
            avatar = add_avatar(uid);
        }
        else
        {
            avatar = it->second;
        }
        stream->read(&avatar->position);
        stream->read(&avatar->color);
    }

    // Remove "dangling" avatars
    // TODO: Fix this along with sync'ing avatars in step function (1/1/2021)
    std::vector<GameInput::UID> uids_to_remove;
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        auto it = std::find_if(uids_seen.begin(), uids_seen.end(), [&](const GameInput::UID &other_uid) { return other_uid == pair.first; });
        if(it == uids_seen.end())
        {
            uids_to_remove.push_back(pair.first);
        }
    }
    while(uids_to_remove.size() > 0)
    {
        remove_avatar(uids_to_remove.back());
        uids_to_remove.pop_back();
    }
}
#endif

#pragma endregion






void GameState::init() { }
void GameState::uninit() { }
void GameState::read_input() { }
void GameState::step(UID focus_uid, float time_step) { }
void GameState::draw() { }
void GameState::serialize(Serialization::Stream *stream, UID uid, bool serialize) { }
void GameState::serialize_input(Serialization::Stream *stream) { }
#if DEBUG
void GameState::draw_debug_ui() { }
#endif



void GameStateMenu::init()
{
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
    // ...
}

void GameStateMenu::step(UID focus_uid, float time_step)
{
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
        Engine::switch_game_mode(GameState::LOBBY);
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
        Engine::switch_game_mode(GameState::EDITOR);
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

void GameStateMenu::serialize(Serialization::Stream *stream, UID uid, bool serialize)
{
}

void GameStateMenu::serialize_input(Serialization::Stream *stream)
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
    frame_number = 0;
    input.avatar_inputs.clear();
    level = Levels::create_level(0);
    local_uid = 0;
}

void GameStateLobby::uninit()
{
    //Levels::destroy_level(playing_level);
    level = nullptr;
}

void GameStateLobby::read_input()
{
    input.avatar_inputs.clear();

    // Read input from local player
    AvatarInput local_input;
    // If we're a client, this field doesn't matter since we're sending
    // an input list to the server, it will just discard the uid
    local_input.uid = local_uid;
    local_input.read_from_local(avatars.find(local_uid));
    input.avatar_inputs.push_back(local_input);

    // If we're a server, read inputs from all clients
    if(Engine::instance->network_mode == Engine::NetworkMode::SERVER)
    {
        // Read remote player's input
        for(Engine::Server::ClientConnection &client : Engine::instance->server.client_connections)
        {
            AvatarInput remote_input;
            remote_input.uid = client.uid;
            remote_input.read_from_connection(&(client.connection));
            if(client.connection != nullptr)
            {
                input.avatar_inputs.push_back(remote_input);
            }
        }
        // Clean out disconnected connections
        Engine::instance->server.remove_disconnected_clients();
    }
}

void GameStateLobby::step(UID focus_uid, float time_step)
{
    frame_number++;

    level->step(time_step);

    avatars.step(input.avatar_inputs, level, time_step);
}

void GameStateLobby::draw()
{
    level->draw();

    // Draw all avatars
    v2 camera_offset = v2(16.0f, 4.0f);
    Avatar *focus_avatar = avatars.find(local_uid);
    if(focus_avatar != nullptr)
    {
        Graphics::Camera::position() = focus_avatar->position + camera_offset;
    }
    Graphics::Camera::width() = 64.0f;

    // Draw the players
    avatars.draw();

    ImGui::Begin("Select level");

    if(ImGui::Button("Level 1"))
    {
        Engine::switch_game_mode(GameState::PLAYING_LEVEL);
        Engine::instance->level_to_load = 1;
    }
    if(ImGui::Button("Level 2"))
    {
        Engine::switch_game_mode(GameState::PLAYING_LEVEL);
        Engine::instance->level_to_load = 2;
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

void GameStateLobby::serialize(Serialization::Stream *stream, UID uid, bool serialize)
{
    if(serialize)
    {
        stream->write(uid);
        stream->write(frame_number);
        stream->write((int)input.avatar_inputs.size());
        for(AvatarInput &input : input.avatar_inputs)
        {
            input.serialize(stream, true);
        }
        level->serialize(stream, true);
    }
    else
    {
        stream->read(&local_uid);
        stream->read(&frame_number);
        int num_inputs;
        stream->read(&num_inputs);
        input.avatar_inputs.resize(num_inputs);
        for(int i = 0; i < num_inputs; i++)
        {
            AvatarInput *target = &(input.avatar_inputs[i]);
            target->serialize(stream, false);
        }
        level->serialize(stream, false);
    }
}

void GameStateLobby::serialize_input(Serialization::Stream *stream)
{
    for(AvatarInput &input : input.avatar_inputs)
    {
        input.serialize(stream, true);
    }
}

#if DEBUG
void GameStateLobby::draw_debug_ui()
{
    //level->draw_debug_ui();
}
#endif



void GameStateLevel::init()
{
    // Initialize state for playing level
    local_uid = 0;
    frame_number = 0;
    input.avatar_inputs.clear();
    level = Levels::create_level(Engine::instance->level_to_load);
}

void GameStateLevel::uninit()
{
    //Levels::destroy_level(playing_level);
    level = nullptr;

    Engine::instance->editing = false;
}

void GameStateLevel::read_input()
{
    input.avatar_inputs.clear();

    // Read input from local player
    AvatarInput local_input;
    // If we're a client, this field doesn't matter since we're sending
    // an input list to the server, it will just discard the uid
    local_input.uid = local_uid;
    local_input.read_from_local(avatars.find(local_uid));
    input.avatar_inputs.push_back(local_input);

    // If we're a server, read inputs from all clients
    if(Engine::instance->network_mode == Engine::NetworkMode::SERVER)
    {
        // Read remote player's input
        for(Engine::Server::ClientConnection &client : Engine::instance->server.client_connections)
        {
            AvatarInput remote_input;
            remote_input.uid = client.uid;
            remote_input.read_from_connection(&(client.connection));
            if(client.connection != nullptr)
            {
                input.avatar_inputs.push_back(remote_input);
            }
        }
        // Clean out disconnected connections
        Engine::instance->server.remove_disconnected_clients();
    }
}

void GameStateLevel::step(UID focus_uid, float time_step)
{
    frame_number++;

    if(Engine::instance->editing)
    {
        level->editor.step(level, time_step);
    }
    else
    {
        level->step(time_step);

        // Step avatars
        avatars.step(input.avatar_inputs, level, time_step);
    }

}

void GameStateLevel::draw()
{
    level->draw();

    // Draw all avatars
    v2 camera_offset = v2(16.0f, 4.0f);
    Avatar *focus_avatar = avatars.find(local_uid);
    if(focus_avatar != nullptr)
    {
        Graphics::Camera::position() = focus_avatar->position + camera_offset;
    }
    Graphics::Camera::width() = 64.0f;

    // Draw the players
    avatars.draw();


    if(Engine::instance->editing)
    {
        level->editor.draw(level);
    }
}

void GameStateLevel::serialize(Serialization::Stream *stream, UID uid, bool serialize)
{
    if(serialize)
    {
        stream->write(uid);
        stream->write(frame_number);
        stream->write((int)input.avatar_inputs.size());
        for(AvatarInput &input : input.avatar_inputs)
        {
            input.serialize(stream, true);
        }
        level->serialize(stream, true);
    }
    else
    {
        stream->read(&local_uid);
        stream->read(&frame_number);
        int num_inputs;
        stream->read(&num_inputs);
        input.avatar_inputs.resize(num_inputs);
        for(int i = 0; i < num_inputs; i++)
        {
            AvatarInput *target = &(input.avatar_inputs[i]);
            target->serialize(stream, false);
        }
        level->serialize(stream, false);
    }
}

void GameStateLevel::serialize_input(Serialization::Stream *stream)
{
    for(AvatarInput &input : input.avatar_inputs)
    {
        input.serialize(stream, true);
    }
}

#if DEBUG
void GameStateLevel::draw_debug_ui()
{
    //level->draw_debug_ui();

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
    Engine::switch_game_mode(GameState::MAIN_MENU);
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
        Engine::switch_game_mode(GameState::PLAYING_LEVEL);
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

void Engine::step_as_client(GameState *game_state, float time_step)
{ 
    // This is before check_for_mode_switch because the connection might update the game mode
    Engine::instance->client.update_connection(time_step);

    Platform::Input::read_input();


    // Send the local player's input to the server
    if(Engine::instance->client.is_connected())
    {
        Serialization::Stream *input_stream = Serialization::make_stream();
        game_state->read_input();
        game_state->serialize_input(input_stream);
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

    if(instance->next_mode != instance->current_mode)
    {
        current_game_state->uninit();
        delete current_game_state;

        switch(instance->next_mode)
        {
        case GameState::Mode::MAIN_MENU:
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

void Engine::switch_game_mode(GameState::Mode mode)
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
