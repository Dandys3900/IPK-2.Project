#ifndef OUTPUTCLASS_H
#define OUTPUTCLASS_H

#include "ConstsFile.h"

using namespace std;

class OutputClass {
    private:
        static std::string get_ip_str (struct sockaddr_in& sock) {
            // Convert ip address back to string
            char ip_buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sock.sin_addr), ip_buf, INET_ADDRSTRLEN);
            return std::string(ip_buf);
        }

        static std::string get_port_str (struct sockaddr_in& sock) {
            return std::to_string(ntohs(sock.sin_port));
        }

    public:
        // Output internal error
        static void out_err_intern (string msg) {
            cerr << string("ERR: " + msg) << endl;
        }
        // Output received message from client
        static void out_recv_msg (struct sockaddr_in& sock, std::string msg_type) {
            cout << string("RECV: " + get_ip_str(sock) + ":" + get_port_str(sock) + "|" + msg_type) << endl;
        }
        // Output sent message to client
        static void out_send_msg (struct sockaddr_in& sock, std::string msg_type) {
            cout << string("SENT: " + get_ip_str(sock) + ":" + get_port_str(sock) + "|" + msg_type) << endl;
        }
        // Output help about how to run the program
        static void out_help () {
            std::string help_text;
            help_text += "Help text:\n";
            help_text += "  -l IP address where server is listening\n";
            help_text += "  -p for specifying port\n";
            help_text += "  -d for UDP timeout [ms]\n";
            help_text += "  -r for UDP retransmissions count";
            // Output to stdout
            cout << help_text << endl;
        }
};

#endif // OUTPUTCLASS_H