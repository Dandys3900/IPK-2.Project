#ifndef SERVERCLASS_H
#define SERVERCLASS_H

#include "ConstsFile.h"
#include "UDPHelper.h"
#include "TCPHelper.h"

#define MAX_WAITING_CONNS 5
#define DEFAULT_CHANNEL "default"

// Struct representing connected client
typedef struct {
    // UDP or TCP when client connects
    CON_TYPE con_type        = NONE;
    // Relevant for UDP only
    uint16_t msg_id          = 0;
    // Storing socket to use for communicating with client
    int client_socket        = 0;
    // Keeps track of current client name and server (display) name
    std::string user_name    = "";
    std::string display_name = "";
    // Initial state
    FSM_STATE state          = S_AUTH;
    // Storing client connection details
    struct sockaddr_in sock_str;
    // Vector storing all already processed messages by their IDs (UDP only)
    std::vector<uint16_t> processed_msgs;
    // Storing names of channel client is connected to
    std::string client_channel = "";
    // Represents queue with messages to be sent to client
    std::queue<std::pair<DataStruct, uint>> msg_queue = {};
    // Boolean for controlling and stopping client thread
    bool stop_thread         = false;
    // Supporting thread for client
    //std::jthread client_thread {};
} ServerClient;

// Struct represeting server channels
typedef struct {
    std::string channel_name = "";
    std::vector<ServerClient> clients;
} ServerChannel;

class Server {
    private:
        // Connection - related
        uint16_t port;
        std::string server_welc_addr;
        struct sockaddr_in server_addr;
        int main_udp_socket;
        int main_tcp_socket;

        // Behaviour - UDP specific
        uint8_t recon_attempts;
        uint16_t timeout;

        // Helper class instances
        UDPHelper* udp_helper;
        TCPHelper* tcp_helper;

        // Protective mutexes
        mutex send_mutex;
        mutex channels_mutex;

        // Decides whether accpet new clients or not
        bool accept_new;

        // Notifies main that server ended and program can terminate
        condition_variable server_cond_var;

        // Vectors for keeping track of all connected users and channels
        std::vector<ServerChannel> channels;
        std::vector<ServerClient> clients;

        std::jthread accept_thread;
        // Vector representing jthread pool
        std::vector<std::jthread> client_threads;

        // Clients managing
        ServerClient create_client (CON_TYPE type, int socket, struct sockaddr_in client_addr);
        void remove_from_channel (ServerClient ending_client);
        void remove_client (ServerClient ending_client);
        // Channels managing
        ServerChannel create_channel (std::string channel_id);
        void add_to_channel (std::string channel_id, ServerClient& new_client);
        auto get_channel (std::string channel_id);
        auto get_client (std::string user_name, std::vector<ServerClient>& pool);

        void bind_connection (int socket);
        void accept_new_clients ();
        void session_end (ServerClient& ending_client);

        // Methods for sending messages to client(s)
        void broadcast_msg (ServerChannel& target, std::string msg, std::string sender);
        void send_msg (ServerClient& to_client, std::string display_name, std::string msg);
        void send_err (ServerClient& to_client, std::string err_msg);
        void send_reply (ServerClient& to_client, uint16_t ref_id, bool result, std::string msg);
        void send_confirm (ServerClient& to_client, uint16_t confirm_to_id);
        void send_bye (ServerClient& to_client);

        void send_message (ServerClient& to_client, DataStruct& data);
        void send_data (ServerClient& to_client, DataStruct& data);

        void switch_to_error (ServerClient& to_client, std::string err_msg);

        std::vector<DataStruct> handle_udp_recv (ServerClient& client);
        std::vector<DataStruct> handle_tcp_recv (ServerClient& client);
        void handle_auth (ServerClient& client, DataStruct auth_msg);
        void handle_client (ServerClient client);

        // Helper methods
        Header create_header (uint8_t type, uint16_t& msg_id);
        bool check_valid (DataStruct& data);
        void set_socket_timeout (int socket_id, uint16_t timeout);

    public:
        Server (std::map<std::string, std::string> data_map);
       ~Server ();

        void start_server ();
        void stop_server ();

        // Getter for conditional variable
        std::condition_variable& get_cond_var () {
            return this->server_cond_var;
        }
        // Indicating server ended for main
        bool server_ended () {
            return this->accept_new == false;
        }
};

#endif // SERVERCLASS_H