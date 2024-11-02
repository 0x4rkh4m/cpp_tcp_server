#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>

enum class Visibility {
    LocalMachine,
    Subnet,
    Network,
    Global
};

class Server {
private:
    // Constants for server configurations
    static constexpr int DEFAULT_PORT = 8080;
    static constexpr int BACKLOG_QUEUE = 5;
    static constexpr int MAX_BUFFER_SIZE = 1024;
    static constexpr const char *LOCALHOST = "127.0.0.1";
    static constexpr const char *GLOBAL = "0.0.0.0";

    // Error and information messages
    static constexpr const char *ERR_SOCKET_CREATION = "[ERROR] Socket creation failed.";
    static constexpr const char *ERR_BIND_FAILED = "[ERROR] Bind failed.";
    static constexpr const char *ERR_LISTEN_FAILED = "[ERROR] Listen failed.";
    static constexpr const char *ERR_ACCEPT_FAILED = "[ERROR] Failed to accept client connection.";
    static constexpr const char *ERR_INVALID_VISIBILITY = "[ERROR] Invalid visibility configuration.";
    static constexpr const char *ERR_SOCKET_OPTIONS = "[ERROR] Failed to set socket options.";
    static constexpr const char *INFO_SERVER_RUNNING = "[INFO] Server is running and ready to accept clients...";
    static constexpr const char *INFO_STOPPING_SERVER = "[INFO] Stopping server and cleaning up resources.";
    static constexpr const char *INFO_SERVER_STOPPED = "[INFO] Server stopped successfully.";
    static constexpr const char *INFO_CLIENT_DISCONNECT = "[INFO] Client disconnected or error in receiving data.";
    static constexpr const char *INFO_VISIBILITY_SET = "[INFO] Server visibility set to ";

    // Server state variables
    int port;
    int server_socket;
    std::string bind_address;
    Visibility visibility;
    std::unordered_map<int, std::string> client_addresses;
    std::vector<std::thread> client_threads;
    std::mutex client_mutex;
    std::atomic<bool> running;

    // Private methods
    bool bind_socket();
    bool listen_socket();
    int accept_client();
    void client_thread(int client_socket);
    bool clean_up();
    std::string get_primary_ip() const;

protected:
    // Virtual handler for handling client requests, can be overridden in subclasses
    virtual bool handle_client(int client_socket);

public:
    explicit Server(Visibility visibility_level = Visibility::LocalMachine);
    virtual ~Server();

    bool configure_visibility(Visibility visibility_level, const std::string &address = "", bool log_info = true);
    bool start_server(int port = DEFAULT_PORT);
    void stop_server();
    void run();
};

#endif // SERVER_H
