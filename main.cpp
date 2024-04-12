#include "ServerClass.h"

// Global variable for chat server
Server* server = nullptr;

void signalHandler (int sig_val) {
    if (sig_val == SIGINT)
        server->stop_server();
}

int main (int argc, char *argv[]) {
    // Map for storing user values
    std::map<std::string, std::string> data_map;

    // Parse cli args
    for (int index = 1; index < argc; ++index) {
        std::string cur_val(argv[index]);
        if (cur_val == std::string("-l"))
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
        else {
            OutputClass::out_err_intern("Unknown flag provided");
            return EXIT_FAILURE;
        }
    }

    // Create Server class instance
    Server server_inst(data_map);
    server = &server_inst;
    // Try starting the server
    try {
        server->start_server();
    } catch (const std::logic_error& e) {
        OutputClass::out_err_intern(std::string(e.what()));
        return EXIT_FAILURE;
    }

    // Set interrput signal handling - CTRL+C
    std::signal(SIGINT, signalHandler);

    mutex end_mutex;
    // Wait for server to finish before returning
    std::unique_lock<std::mutex> lock(end_mutex);
    server->get_cond_var().wait(lock, [] {
        return server->server_ended();
    });

    // End program
    return EXIT_SUCCESS;
}
