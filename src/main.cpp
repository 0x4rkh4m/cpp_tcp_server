#include <csignal>
#include "server.h"
#include <memory>

std::unique_ptr<Server> global_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT && global_server) {
        printf("\nStopping server...\n");
        global_server->stop_server();
    }
}

int main() {
    // Dynamically allocate server
    global_server = std::make_unique<Server>();

    // Set up signal handler for SIGINT
    signal(SIGINT, signal_handler);

    // Configure visibility and start server
    if (global_server->configure_visibility(Visibility::Network) && global_server->start_server()) {
        global_server->run();
    }

    global_server.reset();

    return 0;
}