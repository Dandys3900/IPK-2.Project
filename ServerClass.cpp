#include "ServerClass.h"

Server::Server (std::map<std::string, std::string> data_map)
    : port             (4567),
      server_welc_addr ("0.0.0.0"),
      main_udp_socket  (0),
      main_tcp_socket  (0),
      recon_attempts   (3),
      timeout          (250 /*miliseconds*/),
      accept_new       (true),
      channels         (),
      clients          (),
      client_threads   ()
{
    std::map<std::string, std::string>::iterator iter;
    // Look for user provided-values in map to override init values
    if ((iter = data_map.find("ipaddr")) != data_map.end())
        this->server_welc_addr = iter->second;

    if ((iter = data_map.find("port")) != data_map.end())
        this->port = static_cast<uint16_t>(std::stoi(iter->second));

    if ((iter = data_map.find("reconcount")) != data_map.end())
        this->recon_attempts = static_cast<uint8_t>(std::stoi(iter->second));

    if ((iter = data_map.find("timeout")) != data_map.end())
        this->timeout = static_cast<uint16_t>(std::stoi(iter->second));

    this->udp_helper = new UDPHelper();
    this->tcp_helper = new TCPHelper();

    // Create default channel
    (void)create_channel(DEFAULT_CHANNEL);
}

Server::~Server () {
    delete this->udp_helper;
    delete this->tcp_helper;
}
/***********************************************************************************/
void Server::create_client (CON_TYPE type, int socket, struct sockaddr_in client_addr) {
    // Create new client
    ServerClient new_client = {
        .con_type      = type,
        .client_socket = socket,
        .sock_str      = client_addr,
        .processed_msgs = std::vector<uint16_t>()
    };
    // Start client thread
    this->client_threads.emplace_back(&Server::handle_client, this, new_client);
}
/***********************************************************************************/
ServerChannel Server::create_channel (std::string channel_id) {
    ServerChannel new_channel = {
        .channel_name = channel_id,
        .clients = std::vector<ServerClient>()
    };
    // Return constructed channel
    return new_channel;
}

auto Server::get_channel (std::string channel_id) {
    // Get channel matching user joined channels name
    auto match_channel = std::find_if(this->channels.begin(), this->channels.end(), [&] (const ServerChannel& s) {
        return s.channel_name == channel_id;
    });
    return match_channel;
}

auto Server::get_client (std::string user_name, std::vector<ServerClient>& pool) {
    // Try to find the client
    auto client = std::find_if(pool.begin(), pool.end(), [&] (const ServerClient& c) {
            return c.user_name == user_name;
    });
    return client;
}

void Server::remove_from_channel (ServerClient ending_client) {
    // Get channel matching user joined channels name
    auto match_channel = get_channel(ending_client.client_channel);
    if (match_channel == this->channels.end())
        return;

    // Find client in found channel
    auto toerase_client = get_client(ending_client.user_name, match_channel->clients);
    // Check if user found - erase it
    if (toerase_client != match_channel->clients.end())
        match_channel->clients.erase(toerase_client);

    // Notify other channel clients this user left
    broadcast_msg(*match_channel, std::string(ending_client.user_name + " has " + "left " + match_channel->channel_name), ending_client.user_name);
}

void Server::remove_client (ServerClient ending_client) {
    // Remove from server clients vector
    auto toerase_client = get_client(ending_client.user_name, this->clients);
    if (toerase_client == this->clients.end())
        return;
    // Remove from (any) channel client was in
    remove_from_channel(ending_client);
    // Erase client
    this->clients.erase(toerase_client);
}

void Server::add_to_channel (std::string channel_id, ServerClient& new_client) {
    // Get channel user requested to be joined in
    auto channel = get_channel(channel_id);

    // Remove user from current channel
    remove_from_channel(new_client);

    // Check if channel found - add user
    if (channel != this->channels.end()) {
        std::cout << "Channel found" << std::endl;
        // Notify other server clients this user joined
        broadcast_msg(*channel, std::string(new_client.user_name + " has " + "joined " + channel->channel_name), new_client.user_name);
        // Add to channel
        channel->clients.push_back(new_client);
    }
    else { // Channel not found, create it
        std::cout << "Creating new channel: " << channel_id << std::endl;
        ServerChannel new_channel = create_channel(channel_id);
        new_channel.clients.push_back(new_client);
        // Add new channel to channel vector
        this->channels.push_back(new_channel);
    }

    // Update client's new channel_id
    new_client.client_channel = channel_id;
}
/***********************************************************************************/
void Server::session_end (ServerClient& ending_client) {
    // Close user socket
    if (ending_client.con_type == UDP)
        this->udp_helper->session_end(ending_client.client_socket);
    else // TCP
        this->tcp_helper->session_end(ending_client.client_socket);
    // Stop client thread
    ending_client.stop_thread = true;
    // Remove client
    remove_client(ending_client);
}
/***********************************************************************************/
void Server::bind_connection (int socket) {
    struct sockaddr* addr = (struct sockaddr*)&this->server_addr;
    // Bind to given socket
    if (bind(socket, addr, sizeof(this->server_addr)) < 0)
        throw std::logic_error("Binding failed");
}
/***********************************************************************************/
void Server::start_server () {
    // Make sure everything is reset
    memset(&(this->server_addr), 0, sizeof(this->server_addr));
    // Set domain
    (this->server_addr).sin_family = AF_INET;
    // Set address
    (this->server_addr).sin_addr.s_addr = inet_addr(this->server_welc_addr.c_str());
    // Set port number
    (this->server_addr).sin_port = htons(this->port);

    //************* UDP Connection *************//
    if ((this->main_udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0)
        throw std::logic_error("UDP socket creation failed");

    // Do binding for UDP
    bind_connection(this->main_udp_socket);

    // Set timeout for UDP socket
    set_socket_timeout(this->main_udp_socket, this->timeout);

    //************* TCP Connection *************//
    if ((this->main_tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
        throw std::logic_error("TCP socket creation failed");

    int enable = 1;
    setsockopt(this->main_tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    // Do binding for TCP
    bind_connection(this->main_tcp_socket);

    // Listen for TCP messages
    if (listen(this->main_tcp_socket, MAX_WAITING_CONNS) < 0)
        throw std::logic_error("Error while setting TCP listen");

    // Begin accepting new clients using separete thread
    this->accept_thread = std::jthread(&Server::accept_new_clients, this);
}

void Server::stop_server () {
    // Stop accepting new clients
    this->accept_new = false;
    // Make copy of all server clients and delete original vector to avoid sending broadcasts to other users
    std::vector<ServerClient> copy_clients = this->clients;
    this->clients.clear();

    // Distribute BYE message to all connected clients when stopping the server
    for (auto& client : copy_clients) {
        // Clear clients queue
        client.msg_queue = {};
        send_bye(client);
    }
    // Wait for all clients threads to finish
    for (auto& thread : this->client_threads)
        thread.join();

    //************* UDP Connection *************//
    close(this->main_udp_socket);
    //************* TCP Connection *************//
    shutdown(this->main_tcp_socket, SHUT_RD);
    shutdown(this->main_tcp_socket, SHUT_WR);
    shutdown(this->main_tcp_socket, SHUT_RDWR);
    close(this->main_tcp_socket);

    // Notify main
    this->server_cond_var.notify_one();
}
/***********************************************************************************/
void Server::accept_new_clients () {
    fd_set read_fds;
    int max_fd = std::max(this->main_udp_socket, this->main_tcp_socket);

    while (this->accept_new) {
        FD_ZERO(&read_fds);
        // Set fds for both UDP and TCP sockets
        FD_SET(this->main_udp_socket, &read_fds);
        FD_SET(this->main_tcp_socket, &read_fds);

        // Set timeout to 150ms
        struct timeval time = {
            .tv_sec = 0,
            .tv_usec = suseconds_t(150 * 1000)
        };
        // Use select to wait for activity on either socket
        select(max_fd + 1, &read_fds, nullptr, nullptr, &time);

        // Check if there's a new TCP client
        if (FD_ISSET(this->main_tcp_socket, &read_fds)) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            // Try to accept new incoming client
            int client_socket = accept(this->main_tcp_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket < 0) {
                if (this->accept_new)
                    OutputClass::out_err_intern("Error while accepting TCP client");
                break;
            }
            // Create new client
            create_client(TCP, client_socket, client_addr);
        }

        // Check if there's a new UDP client
        /*if (FD_ISSET(this->main_udp_socket, &read_fds)) {
            // Create new socket for UDP client
            int client_socket;
            if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
                OutputClass::out_err_intern("Creation of UDP socket failed");
                continue;
            }
            // Create new client
            create_client(UDP, client_socket, this->server_addr);
        }*/
    }
}
/***********************************************************************************/
void Server::set_socket_timeout (int socket_id, uint16_t timeout) {
    struct timeval time = {
        .tv_sec = 0,
        .tv_usec = suseconds_t(timeout * 1000)
    };
    // Set timeout for created socket
    if (setsockopt(socket_id, SOL_SOCKET, SO_RCVTIMEO, (char*)&(time), sizeof(struct timeval)) < 0)
        throw std::logic_error("Setting timeout failed");
}
/***********************************************************************************/
void Server::broadcast_msg (ServerChannel& target, std::string msg, std::string sender) {
    std::cout << "Broadcasting msg: " << msg << std::endl;
    // Distribute this message to all clients in given channel
    for (auto& client : target.clients) {
        std::cout << "member of " << target.channel_name << " : " << client.user_name << std::endl;
        // Avoid sending the message to the sender
        if (client.user_name != sender) {
            std::cout << "Broadcast msg sending to: " << client.user_name << " - " << msg << std::endl;
            send_msg(client, target.channel_name, msg);
        }
    }
}
/***********************************************************************************/
bool Server::check_valid (DataStruct& data) {
    switch (data.header.type) {
        case AUTH:
            return regex_match(data.user_name, username_pattern) &&
                   regex_match(data.display_name, display_name_pattern) &&
                   regex_match(data.secret, secret_pattern);
        case ERR:
        case MSG:
            return regex_match(data.display_name, display_name_pattern) &&
                   regex_match(data.message, message_pattern);
        case REPLY:
            return regex_match(data.message, message_pattern);
        case JOIN:
            return regex_match(data.channel_id, channel_id_pattern) &&
                   regex_match(data.display_name, display_name_pattern);
        case BYE:
        case CONFIRM:
            return true;
        default: // Unexpected state, shouldnt happen
            return false;
    }
}
/***********************************************************************************/
Header Server::create_header (uint8_t type, uint16_t& msg_id) {
    // Create header for requested message
    Header header = (Header){
        .type   = type,
        .msg_id = msg_id
    };
    // Avoid useless msg_id incrementation when sending CONFIRM
    if (type != CONFIRM)
        ++msg_id;
    return header;
}
/***********************************************************************************/
void Server::send_msg (ServerClient& to_client, std::string display_name, std::string msg) {
    DataStruct data = {
        .header = create_header(MSG, to_client.msg_id),
        .message = msg,
        .display_name = display_name
    };
    send_message(to_client, data);
}

void Server::send_err (ServerClient& to_client, std::string err_msg) {
    DataStruct data = {
        .header = create_header(ERR, to_client.msg_id),
        .message = err_msg,
        .display_name = to_client.display_name
    };
    send_message(to_client, data);
}

void Server::send_reply (ServerClient& to_client, uint16_t ref_id, bool result, std::string msg) {
    DataStruct data = {
        .header = create_header(REPLY, to_client.msg_id),
        .ref_msg_id = ref_id,
        .result = result,
        .message = msg
    };
    // In case of failed reply, switch to AUTH state
    if (!result)
        to_client.state = S_AUTH;
    send_message(to_client, data);
}

void Server::send_confirm (ServerClient& to_client, uint16_t confirm_to_id) {
    DataStruct data = {
        .header = create_header(CONFIRM, to_client.msg_id),
        .ref_msg_id = confirm_to_id
    };
    send_message(to_client, data);
}

void Server::send_bye (ServerClient& to_client) {
    DataStruct data = {
        .header = create_header(BYE, to_client.msg_id)
    };
    send_message(to_client, data);
}
/* TODO:
    u TCP kdyz se vytvori novy klient a jeste nic neposle a server chce skoncit pres ctrl+c tak to nejde
*/
/***********************************************************************************/
void Server::send_message (ServerClient& to_client, DataStruct& data) {
    DataStruct to_send;
    {
        std::lock_guard<std::mutex> lock(this->send_mutex);
        // Check if message content is valid
        if (!check_valid(data)) {
            OutputClass::out_err_intern("Invalid message content, wont send");
            return;
        }
        // Add message to client queue (recon attempts are 0 for TCP obviously)
        to_client.msg_queue.push({data, ((to_client.con_type == UDP) ? (this->recon_attempts + 1) : 0)});
        // Get front message to send
        to_send = to_client.msg_queue.front().first;
    }
    send_data(to_client, to_send);
}

void Server::send_data (ServerClient& to_client, DataStruct& data) {
    std::lock_guard<std::mutex> lock(this->send_mutex);
    // Prepare data to send
    ssize_t bytes_send;
    const char* out_buffer = NULL;
    std::string message = "";

    if (to_client.con_type == UDP) {
        message = this->udp_helper->convert_to_string(data);
        out_buffer = message.data();
        // Send data
        bytes_send =
            sendto(to_client.client_socket, out_buffer, message.size(), 0, (struct sockaddr*)&(to_client.sock_str), sizeof(to_client.sock_str));
    }
    else { // TCP
        message = this->tcp_helper->convert_to_string(data);
        out_buffer = message.data();
        // Send data
        bytes_send =
            send(to_client.client_socket, out_buffer, message.size(), 0);
    }

    // Check for errors
    if (bytes_send < 0)
        OutputClass::out_err_intern("Error while sending data to client");
    else {
        // Log sent message first
        OutputClass::out_send_msg(to_client.sock_str, this->tcp_helper->get_msg_str(data.header.type));
        // Different handling for TCP
        if (to_client.con_type == TCP) {
            // Remove message from client queue
            to_client.msg_queue.pop();
            // Sent BYE to TCP client -> end connection
            if (data.header.type == BYE)
                session_end(to_client);
        }
    }
}
/***********************************************************************************/
void Server::switch_to_error (ServerClient& to_client, std::string err_msg) {
    // Notify user
    OutputClass::out_err_intern(err_msg);
    // Clear the queue
    to_client.msg_queue = {};
    // Change state - switch to error state
    to_client.state = S_ERROR;
    // Notify client
    send_err(to_client, err_msg);
    // Change state - switch to end state
    to_client.state = S_END;
    // Then send BYE and end
    send_bye(to_client);
}
/***********************************************************************************/
std::vector<DataStruct> Server::handle_udp_recv (ServerClient& client) {
    char in_buffer[MAXLENGTH];
    socklen_t sock_len = sizeof(client.sock_str);
    DataStruct retData;

    while (!client.stop_thread && this->accept_new) {
        // Receive message from client
        ssize_t bytes_received =
            recvfrom(client.client_socket, (char*)in_buffer, MAXLENGTH, 0, (struct sockaddr*)&(client.sock_str), &sock_len);
        // Stop receiving when requested
        if (client.stop_thread || !this->accept_new)
            break;

        // Check for number of received bytes
        if (bytes_received < 3) { // Smaller then compulsory header size (3B)
            // Timeout event
            if ((errno == EWOULDBLOCK || errno == EAGAIN)) {
                // If nothing to send, repeat and wait
                if (client.msg_queue.empty()) {
                    // Expected message from client, none received -> end connection
                    if (client.state == S_AUTH)
                        send_bye(client);
                    continue;
                }
                // Get front message from queue
                auto& msg = client.msg_queue.front();

                // Decide whether resend or not
                if (msg.second > 1) { // Resend
                    // Decrease resend count
                    msg.second -= 1;
                    // Ensure msg_id uniqueness by changing it each time
                    msg.first.header = create_header(msg.first.header.type, client.msg_id);
                    // Resend
                    send_data(client, msg.first);
                }
                else { // No reply from client -> end connection
                    OutputClass::out_err_intern("No response from client, ending connection");
                    session_end(client);
                    break;
                }
            }
            else if (bytes_received < 0) // Output error
                OutputClass::out_err_intern("Error while receiving data from server");
            else // 0 <= size < 3 -> send ERR, BYE and end connection
                switch_to_error(client, "Unsufficient lenght of message received");
            // No reason for processing unsufficient-size response, repeat
            continue;
        }

        // Load message header
        std::memcpy(&retData.header, in_buffer, sizeof(Header));
        // Convert msg_id to correct indian
        retData.header.msg_id = htons(retData.header.msg_id);

        // Confirmation from client event
        if (retData.header.type == CONFIRM) {
            // If nothing to send, repeat and wait
            if (client.msg_queue.empty()) {
                OutputClass::out_err_intern("Unexpected confirmation message received");
                continue;
            }

            // Get front message from queue
            auto& msg = client.msg_queue.front();

            // Check if matching ids
            if (msg.first.header.msg_id == retData.header.msg_id) {
                // Confirmed BYE msg -> end connection
                if (msg.first.header.type == BYE) {
                    session_end(client);
                    break;
                }
                // Confirmed AUTH msg -> join user to default channel
                if (client.state == S_AUTH) {
                    // Fill client details from received AUTH message
                    client.user_name = retData.user_name;
                    client.display_name = retData.display_name;
                    // Switch state
                    client.state = S_OPEN;
                    // Add user to default channel
                    add_to_channel(DEFAULT_CHANNEL, client);
                }
                // Remove confirmed message from queue
                client.msg_queue.pop();
                // Try to send another message from queue
                if (client.msg_queue.empty())
                    continue;
                // Get front message from queue
                auto& msg = client.msg_queue.front();
                // Send
                send_data(client, msg.first);
            }
            else
                OutputClass::out_err_intern("Confirmation to unexpected message received");
            continue;
        }

        // Send confirmation to client
        send_confirm(client, retData.header.msg_id);

        // Check if not already received
        if ((std::find(client.processed_msgs.begin(), client.processed_msgs.end(), retData.header.msg_id)) != client.processed_msgs.end())
            continue;
        // Store and mark as proceeded msg
        client.processed_msgs.push_back(retData.header.msg_id);

        // Try deserialize received message
        try {
            this->udp_helper->deserialize_msg(retData, in_buffer, bytes_received);
            // Check msg validity
            if (!check_valid(retData))
                throw std::logic_error("Invalid message provided");
            break;
        } catch (const std::logic_error& e) {
            // Invalid message from client -> end connection
            switch_to_error(client, e.what());
            continue;
        }
    }
    // Return final vector of received messages
    return std::vector<DataStruct>(1, retData);
}
/***********************************************************************************/
std::vector<DataStruct> Server::handle_tcp_recv (ServerClient& client) {
    char in_buffer[MAXLENGTH];
    size_t msg_shift = 0;
    std::string response;
    DataStruct retData;
    std::vector<DataStruct> ret_vec;

    while (!client.stop_thread && this->accept_new) {
        // Receive message from client
        ssize_t bytes_received =
            recv(client.client_socket, (in_buffer + msg_shift), (MAXLENGTH - msg_shift), 0);
        // Stop receiving when requested
        if (client.stop_thread || !this->accept_new)
            break;

        // Connection (socket) already closed - end
        if (bytes_received <= 0)
            break;

        // Store response to string
        std::string recv_data((in_buffer + msg_shift), bytes_received);
        response += recv_data;
        // Check if message is completed, thus. ending with "\r\n", else continue and wait for rest of message
        if (std::regex_search(recv_data, std::regex("\\r\\n$")) == false) {
            msg_shift += bytes_received;
            continue;
        }

        size_t end_symb_pos = 0;
        // When given buffer contains multiple messages, iterate thorugh them
        while ((end_symb_pos = response.find("\r\n")) != std::string::npos) {
            std::string cur_msg = response.substr(0, end_symb_pos);
            // Move in buffer to another msg (if any)
            response.erase(0, (end_symb_pos + /*delimiter length*/2));
            // Load whole message - each msg ends with "\r\n";
            std::vector<std::string> line_vec = this->tcp_helper->split_to_vec(cur_msg, ' ');

            // Try deserialize received message
            try {
                this->tcp_helper->deserialize_msg(retData, line_vec);
                // Check msg validity
                if (!check_valid(retData))
                    throw std::logic_error("Invalid message provided");
                // Fill client details from received AUTH message
                if (retData.header.type == AUTH) {
                    client.user_name = retData.user_name;
                    client.display_name = retData.display_name;
                }
                // Add message to vector
                ret_vec.push_back(retData);
            } catch (const std::logic_error& e) {
                // Invalid message from client -> end connection
                switch_to_error(client, e.what());
                break;
            }
        }
        break;
    }
    // Return final vector of received messages
    return ret_vec;
}
/***********************************************************************************/
void Server::handle_client (ServerClient client) {
    while (!client.stop_thread && this->accept_new) {
        // Get (vector) of received message(s)
        std::vector<DataStruct> recv_msgs =
            ((client.con_type == UDP) ? handle_udp_recv(client) : handle_tcp_recv(client));
        // Stop receiving when requested
        if (client.stop_thread || !this->accept_new)
            break;
        // Process received messages
        // Iterate through received messages
        for (auto msg : recv_msgs) {
            // Log received message
            OutputClass::out_recv_msg(client.sock_str, this->tcp_helper->get_msg_str(msg.header.type));

            switch (client.state) {
                case S_ACCEPT:
                    if (msg.header.type != AUTH) {
                        if (msg.header.type == BYE)
                            session_end(client);
                        else // Unexpected, user not yet authenticated, just send BYE and end
                            switch_to_error(client, "Unexpected message received");
                        break;
                    }
                    // else handle AUTH message in S_AUTH state below
                case S_AUTH:
                    switch (msg.header.type) {
                        case AUTH:
                            // Check if provided username is unique
                            if (get_client(client.user_name, this->clients) != this->clients.end())
                                send_reply(client, msg.ref_msg_id, false, "Unique username already used");
                            else { // Successful authentization
                                send_reply(client, msg.ref_msg_id, true, "Welcome onboard!");
                                // Add user to server vector of clients
                                this->clients.push_back(client);
                                // For TCP join user directly, for UDP wait for confirmation
                                if (client.con_type == TCP) {
                                    // Fill client details from received AUTH message
                                    client.user_name = msg.user_name;
                                    client.display_name = msg.display_name;
                                    // Switch state
                                    client.state = S_OPEN;
                                    // Add user to default channel
                                    add_to_channel(DEFAULT_CHANNEL, client);
                                }
                            }
                            break;
                        case BYE:
                            send_bye(client);
                            break;
                        default: // Unexpected, switch to error
                            switch_to_error(client, "Unexpected message received");
                            break;
                    }
                    break;
                case S_OPEN:
                    switch (msg.header.type) {
                        case MSG:
                            // Update client display_name
                            client.display_name = msg.display_name;
                            // Broadcast the message to other channel members
                            broadcast_msg(*(get_channel(client.client_channel)), msg.message, client.user_name);
                            break;
                        case JOIN:
                            // Update user display name
                            client.display_name = msg.display_name;
                            // Notify client about succesful connection to channel
                            send_reply(client, msg.ref_msg_id, true, std::string("Succesful join to " + msg.channel_id + " channel"));
                            // Add user to requested channel (or create a new one)
                            add_to_channel(msg.channel_id, client);
                            break;
                        case ERR:
                            // Clear queue
                            client.msg_queue = {};
                            // Send bye
                            send_bye(client);
                            break;
                        case BYE: // End connection
                            session_end(client);
                            break;
                        default: // Unexpected, switch to error
                            switch_to_error(client, "Unexpected message received");
                            break;
                    }
                case S_ERROR:
                case S_END: // Ignore everything
                    break;
                default: // Not expected state, output error
                    OutputClass::out_err_intern("Unknown current server state");
                    break;
            }
        }
    }
}
