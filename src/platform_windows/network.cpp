
#include "network.h"
#include "logging.h"
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
    bool connected;
    SOCKET tcp_socket;
    char ip_address[16];
    int port;
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

static void add_client_connection_to_server(NetworkState *instance, SOCKET client_socket,
        const char *ip_address_string, int port)
{
    Connection *new_connection = &(instance->server.client_connections[instance->server.num_client_connections++]);
    new_connection->tcp_socket = client_socket;
    strcpy(new_connection->ip_address, ip_address_string);
    new_connection->port = port;
    new_connection->connected = true;
}

static void send_stream(Connection *connection, Serialization::Stream *stream)
{
    // Send data over TCP
    char *data = Serialization::stream_data(stream);
    int bytes = Serialization::stream_size(stream);

    long int bytes_queued = send(connection->tcp_socket, data, bytes, 0);
    if(bytes_queued == SOCKET_ERROR)
    {
        Log::log_error("Error sending data: %i\n", get_last_error());
    }

    if(bytes_queued != bytes)
    {
        Log::log_warning("Couldn't queue the requested number of bytes for sending. Requested: %i - Queued: %i", bytes, bytes_queued);
    }

    //return (int)bytes_queued;
}

static bool read_bytes_from_socket(SOCKET socket, char *buffer, int max_bytes_to_read, int *bytes_actually_read)
{
    // Listen for a response
    long int bytes_read_from_socket = recv(socket, buffer, max_bytes_to_read, 0);

    *bytes_actually_read = (int)bytes_read_from_socket;

    // Check if bytes were read
    if(*bytes_actually_read == -1)
    {
        // Bytes weren't read, make sure there's an expected error code
        if(get_last_error() != CODE_WOULD_BLOCK)
        {
            Log::log_error("Error recieving data: %i\n", get_last_error());
        }
        return false;
    }
    else
    {
        // Bytes were read
        return true;
    }
}

static bool read_into_stream(Connection *connection, Serialization::Stream *stream)
{
    bool read_any_data = false;

    // TODO: This can be moved to the instance scope
    static const int RECEIVE_BUFFER_SIZE = 1024 * 2;
    char *receive_buffer = (char *)Platform::Memory::allocate(RECEIVE_BUFFER_SIZE);;

    // Read in entire stream
    bool received_new_data = false;
    do
    {
        // Recieve the data
        int received_bytes;
        received_new_data = read_bytes_from_socket(connection->tcp_socket,
                receive_buffer, RECEIVE_BUFFER_SIZE, &received_bytes);

        if(received_new_data)
        {
            Serialization::write_stream_array(stream, received_bytes, receive_buffer);
            read_any_data = true;
        }

    } while(received_new_data);

    Platform::Memory::free(receive_buffer);
    return read_any_data;
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
        for(int i = 0; i < NetworkState::ServerState::MAX_CLIENTS; i++)
        {
            instance->server.client_connections[i] = {};
        }
    }
}

// Client
void Network::Client::init()
{
    instance->client.server_connection.tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(instance->client.server_connection.tcp_socket == SOCKET_ERROR)
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

    instance->client.server_connection.port = port;

    instance->client.server_connection.connected = true;

    //return error;
}

void Network::Client::send_input_state_to_server()
{
    Connection *server = &(instance->client.server_connection);
    if(!server->connected) return;

    Serialization::Stream *stream = Serialization::make_stream();

    //Input::serialize_input_state(stream);

    send_stream(server, stream);

    Serialization::free_stream(stream);
}

bool Network::Client::read_game_state(Serialization::Stream *stream)
{
    Connection *server = &(instance->client.server_connection);
    if(!server->connected) return false;

    bool read = read_into_stream(server, stream);
    if(read)
    {
        Serialization::reset_stream(stream);
    }
    return read;
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

    Log::log_info("Server listening on port %i...", port);
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
        int port = (int)client_address.sin_port;
        add_client_connection_to_server(instance, incoming_socket, string_buffer, port);

        time_t now;
        time(&now);
        Log::log_info("%s: Client connected on port %i", ctime(&now), port);

    } while(return_code != CODE_WOULD_BLOCK);
}

bool Network::Server::read_client_input_states()
{
    bool read_any_data = false;

    Serialization::Stream *stream = Serialization::make_stream();

    for(int i = 0; i < instance->server.num_client_connections; i++)
    {
        Connection *client_connection = &(instance->server.client_connections[i]);

        if(!client_connection->connected) continue;

        Serialization::clear_stream(stream);
        bool read = read_into_stream(client_connection, stream);
        if(read)
        {
            Serialization::reset_stream(stream);
            //Input::deserialize_input_state(stream);
            read_any_data = true;
        }
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
    return read_any_data;
}

void Network::Server::broadcast_game(Serialization::Stream *game_stream)
{
    for(int i = 0; i < instance->server.num_client_connections; i++)
    {
        Connection *client_connection = &(instance->server.client_connections[i]);
        if(!client_connection->connected) continue;

        send_stream(client_connection, game_stream);
    }
}

