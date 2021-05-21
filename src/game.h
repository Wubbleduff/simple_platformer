
#pragma once

#include "network.h"
#include "game_math.h"
#include "imgui.h"
#include <vector>
#include <array>



struct GameInput
{
    typedef unsigned int UID;
    UID uid = -1;
    enum class Action
    {
        JUMP,
        SHOOT,

        NUM_ACTIONS
    };

    bool current_actions[(int)Action::NUM_ACTIONS] = {};
    float current_horizontal_movement = 0.0f; // Value between -1 and 1
    GameMath::v2 current_aiming_direction = GameMath::v2();

    bool action(Action action);

    void read_from_local(GameMath::v2 avatar_position);
    bool read_from_connection(Network::Connection **connection);
    void serialize(Serialization::Stream *stream, bool serialize);
};
typedef std::vector<GameInput> GameInputList;

struct GameState
{
    enum Mode
    {
        MAIN_MENU,
        LOBBY,
        PLAYING_LEVEL
    };

    Mode mode;
    unsigned int frame_number;
    GameInput::UID local_uid;
    GameInputList inputs_this_frame;

    virtual void init() = 0;
    virtual void uninit();

    virtual void read_input();
    virtual void step(float time_step);
    virtual void draw();
    virtual void serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize);
#if DEBUG
    virtual void draw_debug_ui();
#endif

    void read_input_for_level(struct Level *level); // TODO: Fix this
};

struct GameStateMenu : GameState
{
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
    void step(float time_step);
    void draw();
    void draw_main_menu();
    void draw_join_player();
    void serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize);
#if DEBUG
    void draw_debug_ui();
#endif

    void change_menu(Screen screen);

    static void menu_window_begin();
    static void menu_window_end();
    static ImVec2 button_size();
};

struct GameStateLobby: GameState
{
    struct Level *level;

    void init();
    void uninit();
    void read_input();
    void step(float time_step);
    void draw();
    void serialize(Serialization::Stream *stream, GameInput::UID uid, bool serialize);
#if DEBUG
    void draw_debug_ui();
#endif
};

struct GameStateLevel : GameState
{
    struct Level *playing_level;

    void init();
    void uninit();
    void read_input();
    void step(float time_step);
    void draw();
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
    GameState::Mode current_mode;
    GameState::Mode next_mode;

    bool editing = false;



    static void start();
    static void stop();
    static void init();

    static void switch_game_state(GameState::Mode mode);

    // Switches to client mode and connects to a server
    static void connect(const char *ip_address, int port);

    // Switch to a network mode (local, client, server)
    static void switch_network_mode(NetworkMode mode);

    // Engine tick
    static void do_one_step(float time_step);
    static void step_as_offline(GameState *game_state, float time_step);
    static void step_as_client(GameState *game_state, float time_step);
    static void step_as_server(GameState *game_state, float time_step);
    static void draw_debug_menu();

    static void shutdown();

    
};
