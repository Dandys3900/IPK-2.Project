#include "ConstsFile.h"
#include "UDPClass.h"
#include "TCPClass.h"

// Global variable for storing UDPClass
UDPClass* udpclient = nullptr;

void get_line_words (std::string line, std::vector<std::string>& words_vec) {
    // Clear vector
    words_vec.clear();

    // Create string stream from input line
    std::stringstream ss(line);
    std::string line_word;

    while (ss >> line_word)
        words_vec.push_back(line_word);
}

int main (int argc, char *argv[]) {
    // Not enough args given
    if (argc < 5) {
        OutputClass::out_err_intern("Unsufficient number of arguments provided");
        return EXIT_FAILURE;
    }

    // Map for storing user values
    std::map<std::string, std::string> data_map;

    // Parse cli args
    for (int index = 1; index < argc; ++index) {
        std::string cur_val(argv[index]);
        //if (cur_val == std::string("-t"))
            // TODO - Handling whether use UDP or TCP
        if (cur_val == std::string("-s"))
            data_map.insert({"ipaddr", std::string(argv[++index])});
        else if (cur_val == std::string("-p"))
            data_map.insert({"port", std::string(argv[++index])});
        else if (cur_val == std::string("-d"))
            data_map.insert({"timeout", std::string(argv[++index])});
        else if (cur_val == std::string("-r"))
            data_map.insert({"reconcount", std::string(argv[++index])});
        else if (cur_val == std::string("-h")) {
            // Print help to stdout and exit
            OutputClass::out_help();
            return EXIT_SUCCESS;
        }
        else
            OutputClass::out_err_intern("Unknown flag provided");
    }

    udpclient = new UDPClass(data_map);

    // Process user input
    std::string user_line;
    std::vector<std::string> line_vec;

    // Set interrput signal handling
    std::signal(SIGINT, [](int sig_val){udpclient->send_bye();});

    while (true) {
        // End connection in case of EOF
        if (std::cin.eof()) {
            udpclient->send_bye();
            break;
        }

        std::getline(std::cin, user_line);
        if (user_line.c_str()[0] == '/') {
            // Command - load words from user input line
            get_line_words(user_line, line_vec);
            if (line_vec.at(0) == std::string("/auth") && line_vec.size() == 4) {
                udpclient->send_auth((DataStruct){ .user_name = line_vec.at(1),
                                                   .display_name = line_vec.at(3),
                                                   .secret = line_vec.at(2),});
            }
            else if (line_vec.at(0) == std::string("/join") && line_vec.size() == 2) {
                udpclient->send_join(line_vec.at(1));
            }
            else if (line_vec.at(0) == std::string("/rename") && line_vec.size() == 2) {
                udpclient->send_rename(line_vec.at(1));
            }
            else if (line_vec.at(0) == std::string("/help")) {
                OutputClass::out_help();
            }
            else {
                // Output error and continue
                OutputClass::out_err_intern("Unknown command or unsufficinet number of command params provided");
            }
        }
        else // Msg to send
            udpclient->send_msg(user_line);
    }
    delete udpclient;

    return EXIT_SUCCESS;
}