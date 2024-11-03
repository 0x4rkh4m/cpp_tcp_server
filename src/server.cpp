#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <cstring>

Server::Server(Visibility visibility_level)
        : port(DEFAULT_PORT), server_socket(-1), bind_address(std::string(LOCALHOST)), running(false), client_threads() {
    configure_visibility(visibility_level, "", false);
}

Server::~Server() {
    stop_server();
}

bool Server::configure_visibility(Visibility visibility_level, const std::string &address, bool log_info) {
    switch (visibility_level) {
        case LocalMachine:
            bind_address = std::string(LOCALHOST);
            if (log_info) std::cout << INFO_VISIBILITY_SET << "LocalMachine on " << bind_address << ".\n";
            break;
        case Global:
            bind_address = std::string(GLOBAL);
            if (log_info) std::cout << INFO_VISIBILITY_SET << "Global on all interfaces.\n";
            break;
        case Subnet:
        case Network:
            bind_address = address.empty() ? get_primary_ip() : address;
            if (bind_address.empty()) {
                std::cerr << ERR_BIND_FAILED << " - Could not determine IP for network visibility.\n";
                return false;
            }
            if (log_info) std::cout << INFO_VISIBILITY_SET << (visibility_level == Subnet ? "Subnet" : "Network")
                                    << " on " << bind_address << ".\n";
            break;
        default:
            std::cerr << ERR_INVALID_VISIBILITY << std::endl;
            return false;
    }
    return true;
}

std::string Server::get_primary_ip() {
    struct ifaddrs *ifAddr;
    if (getifaddrs(&ifAddr) == -1) return "";

    std::string primary_ip;
    for (struct ifaddrs *ifa = ifAddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            std::string host(NI_MAXHOST, '\0');
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host.data(), static_cast<socklen_t>(host.size()), nullptr, 0, NI_NUMERICHOST) == 0) {
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
        std::cerr << "[ERROR] Server is not running.\n";
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

    if (int opt = 1; setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
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

bool Server::listen_socket() const {
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
        std::scoped_lock lock(client_mutex);
        client_addresses[client_socket] = inet_ntoa(client_addr.sin_addr);
    } else {
        std::cerr << ERR_ACCEPT_FAILED << ": " << strerror(errno) << std::endl;
    }
    return client_socket;
}

void Server::client_thread(int client_socket, std::stop_token stoken) {
    while (!stoken.stop_requested() && handle_client(client_socket))
    {
        std::scoped_lock lock(client_mutex);
        client_addresses.erase(client_socket);
    }

    close(client_socket);  // Close the client socket after handling it.
}


bool Server::handle_client(int client_socket) {
    std::vector<char> buffer(MAX_BUFFER_SIZE);
    if (ssize_t bytes_read = recv(client_socket, buffer.data(), buffer.size() - 1, 0); bytes_read > 0) {
        send(client_socket, buffer.data(), bytes_read, 0);
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

    for (std::jthread &t : client_threads) {
        if (t.joinable()) t.request_stop();
    }
    std::cout << INFO_SERVER_STOPPED << std::endl;
}

bool Server::clean_up() {
    std::scoped_lock lock(client_mutex);

    for (const auto &[client_socket, client_ip] : client_addresses) {
        std::cout << "[INFO] Closing connection for client: " << client_ip << std::endl;
        close(client_socket);
    }
    client_addresses.clear();

    if (server_socket != -1) {
        close(server_socket);
        server_socket = -1;
    }
    return true;
}

bool Server::is_running() const {
    return running;
}
