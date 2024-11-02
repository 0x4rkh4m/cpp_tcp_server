#include <csignal>
#include "server.h"

Server *global_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT && global_server) {
        printf("\n");
        global_server->stop_server();
    }
}

int main() {
    // Dynamically allocate server and set it to global pointer for signal handling
    global_server = new Server();

    // Set up signal handler for SIGINT
    signal(SIGINT, signal_handler);

    // Configure visibility (set to Network as example) and start server
    if (global_server->configure_visibility(Visibility::Network)) {
        if (global_server->start_server()) {
            global_server->run();
        }
    }

    // Clean up and reset the global server pointer to avoid memory leaks
    delete global_server;
    global_server = nullptr;
    return 0;
}