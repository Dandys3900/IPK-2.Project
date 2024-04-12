#include "UDPHelper.h"

void UDPHelper::session_end (int socket_id) {
    // Close socket
    close(socket_id);
}

void UDPHelper::load_msg_part (const char* input, size_t& input_pos, size_t max_size, std::string& store_to) {
    for (size_t pos = 0; pos < max_size && input[pos] != '\0'; ++pos, ++input_pos)
        store_to += input[pos];
    // Skip currently found null byte
    ++input_pos;
}

void UDPHelper::deserialize_msg (DataStruct& out_str, const char* msg, size_t total_size) {
    size_t msg_pos = sizeof(Header);
    switch (out_str.header.type) {
        // TYPE | MSG_ID | {Username} | {DisplayName} | {Secret}
        case AUTH:
            // Load rest of message
            // User name
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.user_name);
            // Display name
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.display_name);
            // Secret
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.secret);
            break;
        // TYPE | MSG_ID | {ChannelID} | {DisplayName}
        case JOIN:
            // Load rest of message
            // ChannelID
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.channel_id);
            // Display name
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.display_name);
            break;
        // TYPE | MSG_ID | {DisplayName} | {MessageContent}
        case MSG:
        // TYPE | MSG_ID | {DisplayName} | {MessageContent}
        case ERR:
            // Load rest of message
            // Display name
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.display_name);
            // Message
            load_msg_part(msg + msg_pos, msg_pos, (total_size - msg_pos), out_str.message);
            break;
        // TYPE | MSG_ID
        case BYE:
            break;
        // Unknown or invalid message from client
        default:
            throw std::logic_error("Unknown message type provided");
    }
}

std::string UDPHelper::get_str_msg_id (uint16_t msg_id) {
    std::string retval = "";
    // Extract the individual bytes
    char high_byte = static_cast<char>((msg_id >> 8) & 0xFF);
    char low_byte  = static_cast<char>(msg_id & 0xFF);
    // Append the bytes
    retval += high_byte;
    retval += low_byte;
    // Return constructed string holding correct value of msg_id
    return retval;
}

std::string UDPHelper::convert_to_string (DataStruct& data) {
    std::string msg(1, static_cast<char>(data.header.type));
    switch (data.header.type) {
        // TYPE | REFMSG_ID
        case CONFIRM:
            msg += get_str_msg_id(data.ref_msg_id);
            break;
        // TYPE | MSG_ID | {Result} | REFMSG_ID | {MessageContent}
        case REPLY:
            msg += get_str_msg_id(data.header.msg_id) + ((data.result) ? '\x01' : '\x00') + get_str_msg_id(data.ref_msg_id) + data.message + '\0';
            break;
        // TYPE | MSG_ID | {DisplayName} | {MessageContent}
        case MSG:
        // TYPE | MSG_ID | {DisplayName} | {MessageContent}
        case ERR:
            msg += get_str_msg_id(data.header.msg_id) + data.display_name + '\0' + data.message + '\0';
            break;
        // TYPE | MSG_ID
        case BYE:
            msg += get_str_msg_id(data.header.msg_id);
            break;
        // Unknown or invalid message to be created
        default:
            throw std::logic_error("Unknown message type provided");
    }
    // Return composed message
    return msg;
}
