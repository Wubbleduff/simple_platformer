
#include "network.h"
#include "logging.h"
#include "input.h"
#include "platform.h"


#include <WinSock2.h> // Networking API
#include <Ws2tcpip.h> // InetPton
#include <time.h>

#define CODE_WOULD_BLOCK WSAEWOULDBLOCK
#define CODE_IS_CONNECTED WSAEISCONN
#define CODE_INVALID WSAEINVAL

enum BlockingMode
{
    BLOCKING,
    NONBLOCKING
};

struct Connection
{
    SOCKET tcp_socket;
    char ip_address[16];
};

struct NetworkState
{
    struct ClientState
    {
        Connection server_connection;
    };
    ClientState client;

    struct ServerState
    {
        static const int MAX_CLIENTS = 8;
        int num_client_connections;
        Connection *client_connections;

        SOCKET listening_socket;
    };
    ServerState server;

};
NetworkState *Network::instance = nullptr;



static int get_last_error()
{
    return WSAGetLastError();
}

static void init_winsock()
{
    int error;

    // Initialize Winsock
    WSADATA wsa_data;
    error = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if(error) Log::log_error("Error loading winsock: %i\n", get_last_error());
}

static void cleanup_winsock()
{
    // Shut down Winsock
    int error = WSACleanup();
    if(error == -1) Log::log_error("Error unloading networking module: %i\n", get_last_error());
}

static void set_blocking_mode(SOCKET socket, BlockingMode mode)
{
    u_long code = (mode == BLOCKING) ? 0 : 1;
    int error = ioctlsocket(socket, FIONBIO, &code);
}

static void add_client_connection_to_server(NetworkState *instance, SOCKET client_socket, const char *ip_address_string)
{
    Connection *new_connection = &(instance->server.client_connections[instance->server.num_client_connections++]);
    new_connection->tcp_socket = client_socket;
    strcpy(new_connection->ip_address, ip_address_string);
}

static void send_stream(Connection *connection, Serialization::Stream *stream)
{
}

static void read_into_stream(Connection *connection, Serialization::Stream *stream)
{
}



void Network::init()
{
    instance = (NetworkState *)Platform::Memory::allocate(sizeof(NetworkState));

    init_winsock();

    // Client
    {
        instance->client.server_connection = {};
    }

    // Server
    {
        instance->server.num_client_connections = 0;
        instance->server.client_connections =
            (Connection *)Platform::Memory::allocate(sizeof(Connection) * NetworkState::ServerState::MAX_CLIENTS);
    }
}

// Client
void Network::Client::init()
{
    instance->client.server_connection.tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(instance->client.server_connection.tcp_socket == -1)
    {
        Log::log_error("Error opening socket: %i\n", get_last_error());
        return;
    }
}

void Network::Client::connect_to_server(const char *ip_address, int port)
{
    SOCKET &socket = instance->client.server_connection.tcp_socket;

    set_blocking_mode(socket, BLOCKING);

    // Create address
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    int error = inet_pton(AF_INET, ip_address, &address.sin_addr.s_addr);
    if(error == -1)
    {
        Log::log_error("Error creating an address: %i\n", error);
        return;
    }

    // Try to connect
    int code = connect(socket, (sockaddr *)(&address), sizeof(address));
    int status = get_last_error();

    // While not connected, try to reconnect
    while(code != 0 && status != CODE_IS_CONNECTED)
    {
        code = connect(socket, (sockaddr *)(&address), sizeof(address));
        status = get_last_error();

        if(status == CODE_INVALID)
        {
            error = CODE_INVALID;
            Log::log_error("Could not connect to %s:%i", ip_address, port);
            return;
        }
    }

    set_blocking_mode(socket, NONBLOCKING);

    strcpy(instance->client.server_connection.ip_address, ip_address);

    //return error;
}

void Network::Client::send_input_state_to_server()
{
    Serialization::Stream *stream = Serialization::make_stream();

    Input::serialize_input_state(stream);

    send_stream(&(instance->client.server_connection), stream);

    Serialization::free_stream(stream);
}

void Network::Client::read_game_state(Serialization::Stream *stream)
{
    read_into_stream(&(instance->client.server_connection), stream);
}



// Server
void Network::Server::listen_for_client_connections(int port)
{
    // Create a listening socket
    SOCKET listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listening_socket == INVALID_SOCKET)
    {
        Log::log_error("Couldn't create listening socket: %d\n", get_last_error());
        return;
    }

    unsigned long mode = 1; // 1 = Non-blocking, 0 = blocking
    int error = ioctlsocket(listening_socket, FIONBIO, &mode);
    if(error != 0)
    {
        Log::log_error("Couldn't make socket non-blocking: %i\n", get_last_error());
        return;
    }

    sockaddr_in bound_address;
    bound_address.sin_family = AF_INET;
    bound_address.sin_addr.s_addr = INADDR_ANY;
    bound_address.sin_port = htons(port);

    error = bind(listening_socket, (SOCKADDR *)(&bound_address), sizeof(bound_address));
    if(error == SOCKET_ERROR)
    {
        error = get_last_error();
        Log::log_error("Couldn't bind listening socket to port: %d, error: %d\n", port, error);
        return;
    }

    error = listen(listening_socket, SOMAXCONN);
    if(error == SOCKET_ERROR)
    {
        Log::log_error("Couldn't listen on the created socket: %d\n", get_last_error());
        return;
    }

    instance->server.listening_socket = listening_socket;
}

void Network::Server::accept_client_connections()
{
    int return_code = 0;
    do
    {
        if(instance->server.num_client_connections >= NetworkState::ServerState::MAX_CLIENTS) return;

        sockaddr_in client_address;
        int client_address_size = sizeof(sockaddr_in);
        SOCKET incoming_socket = accept(instance->server.listening_socket, (sockaddr *)(&client_address), &client_address_size);
        if(incoming_socket == INVALID_SOCKET)
        {
            return_code = get_last_error();

            // NOTE:
            // This might fail if 2 servers are open on one port

            if(return_code != CODE_WOULD_BLOCK)
            {
                Log::log_error("Error accepting client: %d\n", get_last_error());
            }
            break;
        }

        // Valid socket
        char string_buffer[16];
        const char *client_address_string = inet_ntop(AF_INET, &client_address, string_buffer, sizeof(string_buffer));
        add_client_connection_to_server(instance, incoming_socket, string_buffer);

        time_t now;
        time(&now);
        Log::log_info("Client connected! %s", ctime(&now));

    } while(return_code != CODE_WOULD_BLOCK);
}

void Network::Server::read_client_input_states()
{
    Serialization::Stream *stream = Serialization::make_stream();

    for(int i = 0; i < instance->server.num_client_connections; i++)
    {
        Connection *client_connection = &(instance->server.client_connections[i]);

        Serialization::clear_stream(stream);
        read_into_stream(client_connection, stream);
        Serialization::reset_stream(stream);
        Input::deserialize_input_state(stream);
    }

    // DEBUG
#if 0
    {
        static int count = 0;
        count++;
        for(int i = 0; i < 256; i++)
        {
            int value = 0;
            if(i == 'D') value = 1;
            if(i == ' ') value = 1;
            Serialization::write_stream(stream, value);
        }
        for(int i = 0; i < 256; i++) Serialization::write_stream(stream, 0);
        for(int i = 0; i < 8; i++) Serialization::write_stream(stream, 0);
        for(int i = 0; i < 8; i++) Serialization::write_stream(stream, 0);

        Serialization::reset_stream(stream);

        Input::deserialize_input_state(stream);
    }
#endif

    Serialization::free_stream(stream);
}

void Network::Server::broadcast_game_state(Serialization::Stream *game_state_stream)
{
    /*
    for(int i = 0; i < instance->num_client_connections; i++)
    {
        send_stream(instance->client_connections[i], game_state_stream)
    }
    */
}

