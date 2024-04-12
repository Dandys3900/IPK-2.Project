#ifndef UDPHELPERCLASS_H
#define UDPHELPERCLASS_H

#include "ConstsFile.h"

class UDPHelper {
    public:
        UDPHelper () {};
       ~UDPHelper () {};
        // Helper methods
        void session_end (int socket_id);
        void load_msg_part (const char* input, size_t& input_pos, size_t max_size, std::string& store_to);
        void deserialize_msg (DataStruct& out_str, const char* msg, size_t total_size);
        std::string get_str_msg_id (uint16_t msg_id);
        std::string convert_to_string (DataStruct& data);
};

#endif // UDPHELPERCLASS_H