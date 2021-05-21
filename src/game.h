
#pragma once

#include "network.h"
#include "game_math.h"
#include "levels.h"
#include "imgui.h"
#include <vector>
#include <array>
#include <map>



/*
struct GameInput
{
    UID uid;
    enum class Action
    {
        JUMP,
        SHOOT,

        NUM_ACTIONS
    };

    bool current_actions[(int)Action::NUM_ACTIONS] ={};
    float current_horizontal_movement; // Value between -1 and 1
    GameMath::v2 current_aiming_direction;

    bool action(Action action);

    void read_from_local(GameMath::v2 avatar_position);
    bool read_from_connection(Network::Connection **connection);
    void serialize(Serialization::Stream *stream, bool serialize);
};
*/

typedef unsigned int UID;
struct MenuInput
{
    GameMath::v2 mouse_position;
    bool mouse_down;
    bool select;
    bool direction_input[4];

    void read_from_local();
    //bool read_from_connection(Network::Connection **connection); // Does this need to be serialized?
    void serialize(Serialization::Stream *stream, bool serialize);
};
struct AvatarInput
{
    UID uid;
    float horizontal_movement;
    bool jump;
    GameMath::v2 aiming_direction;
    bool shoot;

    void read_from_local(struct Avatar *avatar);
    bool read_from_connection(Network::Connection **connection);
    void serialize(Serialization::Stream *stream, bool serialize);
};
typedef std::vector<AvatarInput> AvatarInputList;

struct Avatar
{
    GameMath::v2 position;
    GameMath::v4 color;
    bool grounded;
    float horizontal_velocity;
    float vertical_velocity;
    float run_strength;
    float friction_strength;
    float mass;
    float gravity;
    float full_extent;

    void reset(struct Level *level);
    void step(AvatarInput *input, struct Level *level, float time_step);
    void draw();
    void check_and_resolve_collisions(struct Level *level);
};

struct AvatarList
{
    std::map<UID, Avatar *> avatar_map;

    Avatar *add(UID uid, Level *level);
    Avatar *find(UID uid);
    void remove(UID uid);
    void sync_with_inputs(AvatarInputList inputs, Level *level);

    void step(AvatarInputList inputs, struct Level *level, float time_step);
    void draw();
};

struct GameState
{
    enum Mode
    {
        MAIN_MENU,
        LOBBY,
        PLAYING_LEVEL,
        EDITOR,
    } mode;

    virtual void init() = 0;
    virtual void uninit();

    virtual void read_input();
    virtual void step(UID focus_uid, float time_step);
    virtual void draw();
    virtual void serialize(Serialization::Stream *stream, UID uid, bool serialize);
    virtual void serialize_input(Serialization::Stream *stream); // For clients to send to servers
#if DEBUG
    virtual void draw_debug_ui();
#endif
};

struct GameStateMenu : GameState
{
    struct Input
    {
        MenuInput menu_input;
    } input;

    enum Screen
    {
        MAIN_MENU,
        JOIN_PLAYER
    } screen = MAIN_MENU;
    bool confirming_quit_game = false;
    bool maybe_show_has_timed_out = false;
    static const ImGuiWindowFlags IMGUI_MENU_WINDOW_FLAGS =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_MenuBar    | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoCollapse   |
        ImGuiWindowFlags_NoNav      | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    void init();
    void uninit();
    void read_input();
    void step(UID focus_uid, float time_step);
    void draw();
    void serialize(Serialization::Stream *stream, UID uid, bool serialize);
    void serialize_input(Serialization::Stream *stream);
#if DEBUG
    void draw_debug_ui();
#endif

    void change_menu(Screen screen);
    void draw_main_menu();
    void draw_join_player();

    static void menu_window_begin();
    static void menu_window_end();
    static ImVec2 button_size();

};

struct GameStateLobby: GameState
{
    struct Input
    {
        MenuInput menu_input;
        AvatarInputList avatar_inputs;
    } input;

    int frame_number;
    UID local_uid;
    AvatarList avatars;
    struct Level *level;

    void init();
    void uninit();
    void read_input();
    void step(UID focus_uid, float time_step);
    void draw();
    void serialize(Serialization::Stream *stream, UID uid, bool serialize);
    void serialize_input(Serialization::Stream *stream);
#if DEBUG
    void draw_debug_ui();
#endif
};

struct GameStateLevel : GameState
{
    struct Input
    {
        MenuInput menu_input; // For pause menu
        AvatarInputList avatar_inputs;
    } input;

    int frame_number;
    UID local_uid;
    AvatarList avatars;
    struct Level *level;
    
    void init();
    void uninit();
    void read_input();
    void step(UID focus_uid, float time_step);
    void draw();
    void serialize(Serialization::Stream *stream, UID uid, bool serialize);
    void serialize_input(Serialization::Stream *stream);
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
            UID uid = 0;
            Network::Connection *connection = nullptr;
        };
        std::vector<ClientConnection> client_connections;
        UID next_input_uid = 1;

        bool startup(int port);
        void shutdown();
        void add_client_connection(Network::Connection *connection);
        void remove_disconnected_clients();
    } server;

    GameState *current_game_state;
    GameState::Mode current_mode;
    GameState::Mode next_mode;
    bool editing = false;
    int level_to_load = 0;



    static void start();
    static void stop();
    static void init();

    static void switch_game_mode(GameState::Mode mode);

    static void exit_to_main_menu();

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


