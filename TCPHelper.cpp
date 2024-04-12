#include "TCPHelper.h"

void TCPHelper::session_end(int socket_id) {
    // Clear sockets
    shutdown(socket_id, SHUT_RD);
    shutdown(socket_id, SHUT_WR);
    shutdown(socket_id, SHUT_RDWR);
    // Close socket
    close(socket_id);
}

MSG_TYPE TCPHelper::get_msg_type(std::string first_msg_word) {
    if (std::regex_match(first_msg_word, std::regex("^AUTH$", std::regex_constants::icase)))
        return AUTH;
    if (std::regex_match(first_msg_word, std::regex("^JOIN$", std::regex_constants::icase)))
        return JOIN;
    if (std::regex_match(first_msg_word, std::regex("^MSG$", std::regex_constants::icase)))
        return MSG;
    if (std::regex_match(first_msg_word, std::regex("^ERR$", std::regex_constants::icase)))
        return ERR;
    if (std::regex_match(first_msg_word, std::regex("^BYE$", std::regex_constants::icase)))
        return BYE;
    return NO_TYPE;
}

std::string TCPHelper::get_msg_str(uint8_t type) {
    switch (type) {
        case AUTH:
            return "AUTH";
        case REPLY:
            return "REPLY";
        case JOIN:
            return "JOIN";
        case MSG:
            return "MSG";
        case ERR:
            return "ERR";
        case BYE:
            return "BYE";
        case CONFIRM:
            return "CONFIRM";
        default:
            return "";
    }
}

std::string TCPHelper::load_msg_part(size_t start_from, std::vector<std::string>& line_vec) {
    std::string out = "";
    // Concatenate the rest of words in vector as message content
    for (size_t start_ind = start_from; start_ind < line_vec.size(); ++start_ind) {
        if (start_ind != start_from)
            out += " ";
        out += line_vec.at(start_ind);
    }
    return out;
}

void TCPHelper::deserialize_msg(DataStruct& out_str, std::vector<std::string>& line_vec) {
    out_str.header.type = get_msg_type(line_vec.at(0));
    switch (out_str.header.type) {
        // AUTH {Username} AS {DisplayName} USING {Secret}\r\n
        case AUTH:
            if (line_vec.size() < 6)
                throw std::logic_error("Unsufficient lenght of AUTH message received");
            // Load rest of message
            out_str.user_name    = line_vec.at(1);
            out_str.display_name = line_vec.at(3);
            out_str.secret       = line_vec.at(5);
            break;
        // JOIN {ChannelID} AS {DisplayName}\r\n
        case JOIN:
            if (line_vec.size() < 4)
                throw std::logic_error("Unsufficient lenght of JOIN message received");
            // Load rest of message
            out_str.channel_id   = line_vec.at(1);
            out_str.display_name = line_vec.at(3);
            break;
        // MSG FROM {DisplayName} IS {MessageContent}\r\n
        case MSG:
        // ERR FROM {DisplayName} IS {MessageContent}\r\n
        case ERR:
            if (line_vec.size() < 4)
                throw std::logic_error("Unsufficient lenght of ERR/MSG message received");
            // Load rest of message
            out_str.display_name = line_vec.at(2);
            out_str.message      = load_msg_part(4, line_vec);
            break;
        // BYE\r\n
        case BYE:
            break;
        // Unknown or invalid message from client
        default:
            throw std::logic_error("Unknown message type provided");
    }
}

std::string TCPHelper::convert_to_string(DataStruct &data) {
    std::string msg = "";
    switch (data.header.type) {
        // REPLY {"OK"|"NOK"} IS {MessageContent}\r\n
        case REPLY:
            msg = "REPLY " + std::string((data.result) ? "OK" : "NOK") + " IS " + data.message + "\r\n";
            break;
        // MSG FROM {DisplayName} IS {MessageContent}\r\n
        case MSG:
            msg = "MSG FROM " + data.display_name + " IS " + data.message + "\r\n";
            break;
        // ERR FROM {DisplayName} IS {MessageContent}\r\n
        case ERR:
            msg = "ERR FROM " + data.display_name + " IS " + data.message + "\r\n";
            break;
        // BYE\r\n
        case BYE:
            msg = "BYE\r\n";
            break;
        // Unknown or invalid message to be created
        default:
            throw std::logic_error("Unknown message type provided");
    }
    // Return composed message
    return msg;
}

std::vector<std::string> TCPHelper::split_to_vec (std::string line, char delim) {
    std::vector<std::string> words_vec;

    // Create string stream from input line
    std::stringstream ss(line);
    std::string line_word;

    // Insert each word to vector
    while(getline(ss, line_word, delim))
        words_vec.push_back(line_word);
    return words_vec;
}
