#ifndef CONSTSFILE_H
#define CONSTSFILE_H

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <algorithm>
#include <map>
#include <csignal>
#include <thread>
#include <regex>
#include <queue>
#include <mutex>
#include <stdexcept>
#include <condition_variable>
#include <sys/select.h>

#include "OutputClass.h"

#define MAXLENGTH 2048

// Regex patterns for message values check
const regex username_pattern     ("^[A-z0-9-]{1,20}$");
const regex channel_id_pattern   ("^[A-z0-9-.]{1,20}$");
const regex secret_pattern       ("^[A-z0-9-]{1,128}$");
const regex display_name_pattern ("^[\x21-\x7E]{1,20}$");
const regex message_pattern      ("^[\x20-\x7E]{1,1400}$");

// Enum for message types
enum MSG_TYPE : uint8_t {
    NO_TYPE  = 0x05,
    CONFIRM  = 0x00,
    REPLY    = 0x01,
    AUTH     = 0x02,
    JOIN     = 0x03,
    MSG      = 0x04,
    ERR      = 0xFE,
    BYE      = 0xFF
};

// Enum for client states
enum FSM_STATE : uint8_t {
    S_ACCEPT = 0,
    S_AUTH,
    S_OPEN,
    S_ERROR,
    S_END
};

// Enum for possible thread events
enum THREAD_EVENT {
    NO_EVENT = 0,
    TIMEOUT,
    CONFIRMATION
};

// Enum for available connection types
enum CON_TYPE {
    NONE = 0,
    TCP,
    UDP
};

#pragma pack(push, 1)
typedef struct {
    uint8_t type    = NO_TYPE; // 1 byte
    uint16_t msg_id = 0;       // 2 bytes
} Header;
#pragma pack(pop)

typedef struct {
    Header header;                      // 3 bytes (msg_type + msg_id)
    uint16_t ref_msg_id      = 0;       // 2 bytes
    bool result              = false;   // 1 byte
    std::string message      = "";      // N bytes
    std::string user_name    = "";      // N bytes
    std::string display_name = "";      // N bytes
    std::string secret       = "";      // N bytes
    std::string channel_id   = "";      // N bytes
    bool sent                = false;   // 1 bytes
} DataStruct;

#endif // CONSTSFILE_H