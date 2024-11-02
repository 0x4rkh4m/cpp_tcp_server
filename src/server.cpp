#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <iostream>
#include <cstring>

Server::Server(Visibility visibility_level)
        : port(DEFAULT_PORT), server_socket(-1), running(false), visibility(visibility_level), bind_address(LOCALHOST) {
    configure_visibility(visibility_level, "", false);
}

Server::~Server() {
    stop_server();
}

bool Server::configure_visibility(Visibility visibility_level, const std::string &address, bool log_info) {
    visibility = visibility_level;
    switch (visibility_level) {
        case Visibility::LocalMachine:
            bind_address = LOCALHOST;
            if (log_info) std::cout << INFO_VISIBILITY_SET << "LocalMachine on " << bind_address << "." << std::endl;
            break;
        case Visibility::Global:
            bind_address = GLOBAL;
            if (log_info) std::cout << INFO_VISIBILITY_SET << "Global on all interfaces." << std::endl;
            break;
        case Visibility::Subnet:
        case Visibility::Network:
            bind_address = address.empty() ? get_primary_ip() : address;
            if (bind_address.empty()) {
                std::cerr << ERR_BIND_FAILED << " - Could not determine IP for network visibility." << std::endl;
                return false;
            }
            if (log_info) std::cout << INFO_VISIBILITY_SET
                                    << (visibility_level == Visibility::Subnet ? "Subnet" : "Network")
                                    << " on " << bind_address << "." << std::endl;
            break;
        default:
            std::cerr << ERR_INVALID_VISIBILITY << std::endl;
            return false;
    }
    return true;
}

std::string Server::get_primary_ip() const {
    struct ifaddrs *ifAddr, *ifa;
    std::string primary_ip;

    if (getifaddrs(&ifAddr) == -1) return "";

    for (ifa = ifAddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            char host[NI_MAXHOST];
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0) {
                primary_ip = host;
                break;
            }
        }
    }
    freeifaddrs(ifAddr);
    return primary_ip;
}

bool Server::start_server(int port) {
    this->port = port;
    running = true;
    return bind_socket() && listen_socket();
}

void Server::run() {
    if (!running) {
        std::cerr << "[ERROR] Server is not running." << std::endl;
        return;
    }

    std::cout << INFO_SERVER_RUNNING << std::endl;

    while (running) {
        int client_socket = accept_client();
        if (client_socket >= 0) {
            client_threads.emplace_back(&Server::client_thread, this, client_socket);
        }
    }
}

bool Server::bind_socket() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << ERR_SOCKET_CREATION << ": " << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << ERR_SOCKET_OPTIONS << ": " << strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, bind_address.c_str(), &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << ERR_BIND_FAILED << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool Server::listen_socket() {
    if (listen(server_socket, BACKLOG_QUEUE) < 0) {
        std::cerr << ERR_LISTEN_FAILED << ": " << strerror(errno) << std::endl;
        return false;
    }
    std::cout << "[INFO] Server started on port " << port << std::endl;
    return true;
}

int Server::accept_client() {
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_socket >= 0) {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_addresses[client_socket] = inet_ntoa(client_addr.sin_addr);
    } else {
        std::cerr << ERR_ACCEPT_FAILED << ": " << strerror(errno) << std::endl;
    }
    return client_socket;
}

void Server::client_thread(int client_socket) {
    while (running && handle_client(client_socket)) {}
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_addresses.erase(client_socket);
    }
    close(client_socket);
}

bool Server::handle_client(int client_socket) {
    char buffer[MAX_BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        send(client_socket, buffer, bytes_read, 0);
    } else {
        std::cerr << INFO_CLIENT_DISCONNECT << std::endl;
        return false;
    }
    return true;
}

void Server::stop_server() {
    if (!running) return;
    running = false;

    std::cout << INFO_STOPPING_SERVER << std::endl;

    clean_up();

    for (std::thread &t : client_threads) {
        if (t.joinable()) t.join();
    }

    std::cout << INFO_SERVER_STOPPED << std::endl;
}

bool Server::clean_up() {
    std::lock_guard<std::mutex> lock(client_mutex);

    for (const auto &[client_socket, client_ip] : client_addresses) {
        std::cout << "[INFO] Closing connection with client " << client_ip << " (socket " << client_socket << ")." << std::endl;
        close(client_socket);
    }
    client_addresses.clear();

    if (server_socket >= 0) {
        std::cout << "[INFO] Closing main server socket " << server_socket << "." << std::endl;
        close(server_socket);
        server_socket = -1;
    }
    return true;
}
