
#include "network.h"
#include "logging.h"
#include "platform.h"
#include "data_structures.h"


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



static int get_last_error()
{
    return WSAGetLastError();
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
            Log::log_error("Error receiving data: %i\n", get_last_error());
        }
        return false;
    }
    else
    {
        // Bytes were read
        return true;
    }
}

static void set_blocking_mode(SOCKET socket, BlockingMode mode)
{
    u_long code = (mode == BLOCKING) ? 0 : 1;
    int error = ioctlsocket(socket, FIONBIO, &code);
}

struct Connection
{
    bool connected;
    SOCKET tcp_socket;
    char ip_address[16];
    int port;



    struct Header
    {
        int content_size;
    };

    static const int RECEIVE_BUFFER_SIZE = 1024 * 2;
    union
    {
        char *receive_buffer;
        Header *buffer_header;
    };
    char *receive_target;
    static const int HEADER_SIZE = sizeof(Header);



    void disconnect()
    {
        connected = false;

        set_blocking_mode(tcp_socket, BLOCKING);

        int result = ::shutdown(tcp_socket, SD_SEND);
        if(result == SOCKET_ERROR)
        {
            Log::log_error("Could not end connection to server properly");
            return;
        }

        while(true)
        {
            int received_bytes = 0;
            bool success = read_bytes_from_socket(tcp_socket,
                receive_buffer, RECEIVE_BUFFER_SIZE, &received_bytes);
            if(!success) break;
            if(success && received_bytes == 0) break;
        }

        closesocket(tcp_socket);
    }

    void send_stream(Serialization::Stream *stream)
    {
        // Send data over TCP
        char *data = Serialization::stream_data(stream);
        int bytes = Serialization::stream_size(stream);

        long int bytes_queued = send(tcp_socket, data, bytes, 0);
        if(bytes_queued == SOCKET_ERROR)
        {
            Log::log_error("Error sending data: %i\n", get_last_error());
        }

        if(bytes_queued != bytes)
        {
            Log::log_warning("Couldn't queue the requested number of bytes for sending. Requested: %i - Queued: %i", bytes, bytes_queued);
        }
    }




    bool expecting_header()
    {
        char *content = receive_buffer + Connection::HEADER_SIZE;
        return (receive_target < content);
    }

    void try_receive_data(int receive_up_to_bytes)
    {
        char **receive_buffer_target = &(receive_buffer);
        const char *receive_buffer_end = receive_buffer + receive_up_to_bytes;

        bool received_new_data = true;
        while(received_new_data && *receive_buffer_target != receive_buffer_end)
        {
            // Recieve the data
            int bytes_to_receive = receive_buffer_end - *receive_buffer_target;
            int received_bytes;
            received_new_data = read_bytes_from_socket(tcp_socket,
                    *receive_buffer_target, bytes_to_receive, &received_bytes);

            if(received_new_data)
            {
                if(received_bytes == 0)
                {
                    // Client closed the connection
                    connected = false;
                    return;
                }
                else
                {
                    *receive_buffer_target += received_bytes;
                }
            }
        }
    }

    void update_receive_state()
    {
        if(expecting_header())
        {
            try_receive_data(HEADER_SIZE);
            if(!connected) return;
        }
        else
        {
            try_receive_data(HEADER_SIZE + buffer_header->content_size);
            if(!connected) return;
        }
    }

    void reset_receive_state()
    {
        receive_target = receive_buffer;
    }

    bool ready_to_read()
    {
        if(expecting_header()) return false;

        char *receive_end = receive_buffer + HEADER_SIZE + buffer_header->content_size;
        return receive_target == receive_end;
    }

    void read_into_stream(Serialization::Stream *stream)
    {
        int content_bytes = buffer_header->content_size;
        char *content = receive_buffer + HEADER_SIZE;
        Serialization::write_stream_array(stream, content_bytes, content);
        reset_receive_state();
    }
};



struct NetworkState
{
    struct ClientState
    {
        Connection *server_connection;
    };
    ClientState client;

    struct ServerState
    {
        DynamicArray<Connection *> client_connections;
        SOCKET listening_socket;
    };
    ServerState server;
};
NetworkState *Network::instance = nullptr;




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

static Connection *make_connection()
{
    Connection *new_connection = (Connection *)Platform::Memory::allocate(sizeof(Connection));
    new_connection->connected = false;
    new_connection->tcp_socket = INVALID_SOCKET;
    Platform::Memory::memset(new_connection->ip_address, 0, sizeof(new_connection->ip_address));
    new_connection->port = 0;

    return new_connection;
}

static void destroy_connection(Connection **connection)
{
    (*connection)->disconnect();
    Platform::Memory::free(*connection);
    *connection = nullptr;
}









void Network::init()
{
    instance = (NetworkState *)Platform::Memory::allocate(sizeof(NetworkState));

    init_winsock();

    // Client
    {
        instance->client.server_connection = nullptr;
    }

    // Server
    {
        instance->server.client_connections.init();
        instance->server.listening_socket = INVALID_SOCKET;
    }
}

// Client
void Network::Client::disconnect()
{
    if(!instance->client.server_connection->connected) return;

    destroy_connection(&(instance->client.server_connection));
}

void Network::Client::connect_to_server(const char *ip_address, int port)
{
    instance->client.server_connection = make_connection();

    SOCKET &client_socket = instance->client.server_connection->tcp_socket;

    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client_socket == SOCKET_ERROR)
    {
        Log::log_error("Error opening socket: %i\n", get_last_error());
        return;
    }

    set_blocking_mode(client_socket, BLOCKING);

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
    int code = connect(client_socket, (sockaddr *)(&address), sizeof(address));
    int status = get_last_error();

    // While not connected, try to reconnect
    while(code != 0 && status != CODE_IS_CONNECTED)
    {
        code = connect(client_socket, (sockaddr *)(&address), sizeof(address));
        status = get_last_error();

        if(status == CODE_INVALID)
        {
            error = CODE_INVALID;
            Log::log_error("Could not connect to %s:%i", ip_address, port);
            return;
        }
    }

    set_blocking_mode(client_socket, NONBLOCKING);

    strcpy(instance->client.server_connection->ip_address, ip_address);
    instance->client.server_connection->port = port;
    instance->client.server_connection->connected = true;
}

void Network::Client::send_input_state_to_server()
{
    Connection *server = instance->client.server_connection;
    if(server && server->connected)
    {
        Serialization::Stream *stream = Serialization::make_stream();

        //Input::serialize_input_state(stream);

        server->send_stream(stream);

        Serialization::free_stream(stream);
    }
}

Network::ReadResult Network::Client::read_game_state(Serialization::Stream *stream)
{
    Connection *server = instance->client.server_connection;
    if(!server || !server->connected)
    {
        return Network::ReadResult::CLOSED;
    }

    // Update the connection data (read into buffer) and state (closed or still open)
    server->update_receive_state();

    // Check if the connection was closed after the read
    if(!server->connected)
    {
        destroy_connection(&(instance->client.server_connection));
        return Network::ReadResult::CLOSED;
    }

    // Check if game data is ready to be read
    if(server->ready_to_read())
    {
        server->read_into_stream(stream);
        return Network::ReadResult::READY;
    }
    else
    {
        return Network::ReadResult::NOT_READY;
    }
}



// Server
void Network::Server::disconnect()
{
    Connection *server = instance->client.server_connection;
    if(!server || !server->connected) return;

    for(int i = 0; i < instance->server.client_connections.size; i++)
    {
        Connection *client = instance->server.client_connections[i];
        destroy_connection(&(instance->server.client_connections[i]));
    }

    instance->server.client_connections.clear();
}

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
        Connection *client_connection = make_connection();
        client_connection->tcp_socket = incoming_socket;
        char string_buffer[16];
        client_connection->connected = true;
        inet_ntop(AF_INET, &client_address, client_connection->ip_address, sizeof(client_connection->ip_address));
        client_connection->port = (int)client_address.sin_port;
        instance->server.client_connections.push_back(client_connection);

        time_t now;
        time(&now);
        Log::log_info("%s: Client connected on port %i", ctime(&now), client_connection->port);

    } while(return_code != CODE_WOULD_BLOCK);
}

Network::ReadResult Network::Server::read_client_input_states()
{
    Serialization::Stream *stream = Serialization::make_stream();

    for(int i = 0; i < instance->server.client_connections.size; i++)
    {
        Connection *client = instance->server.client_connections[i];

        if(!client->connected) continue;

        // Update the connection data (read into buffer) and state (closed or still open)
        client->update_receive_state();

        // Check if the connection was closed after the read
        if(!client->connected)
        {
            //return Network::ReadResult::CLOSED;
            Log::log_info("Client disconnected");
            destroy_connection(&(instance->server.client_connections[i]));
            continue;
        }

        // Check if game data is ready to be read
        if(client->ready_to_read())
        {
            //client->read_into_stream(stream);
            //return Network::ReadResult::READY;
        }
        else
        {
            //return Network::ReadResult::NOT_READY;
        }
    }

    // Clear all disconnected clients
    for(int i = 0; i < instance->server.client_connections.size; i++)
    {
        if(instance->server.client_connections[i] == nullptr)
        {
            // Safe in loop
            instance->server.client_connections.remove_unordered(i);
        }
    }

    Serialization::free_stream(stream);

    return ReadResult::CLOSED;
}

void Network::Server::broadcast_game(Serialization::Stream *game_stream)
{
    for(int i = 0; i < instance->server.client_connections.size; i++)
    {
        Connection *client_connection = instance->server.client_connections[i];
        if(!client_connection->connected) continue;

        //client_connection->send_stream(game_stream);
    }
}

