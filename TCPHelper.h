#ifndef TCPHELPERCLASS_H
#define TCPHELPERCLASS_H

#include "ConstsFile.h"

class TCPHelper {
    public:
        TCPHelper () {};
       ~TCPHelper () {};
        // Helper methods
        void session_end (int socket_id);
        MSG_TYPE get_msg_type (std::string first_msg_word);
        std::string get_msg_str (uint8_t type);
        std::string load_msg_part (size_t start_from, std::vector<std::string>& line_vec);
        void deserialize_msg (DataStruct& out_str, std::vector<std::string>& line_vec);
        std::string convert_to_string (DataStruct &data);
        std::vector<std::string> split_to_vec (std::string line, char delim);
};

#endif // TCPHELPERCLASS_H