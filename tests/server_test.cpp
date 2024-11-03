#include "server.h"
#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <memory>
#include <string>

class ServerTest : public ::testing::Test {
protected:
    std::unique_ptr<Server> server;
    const int test_port = 9090;
    static constexpr size_t MAX_TEST_BUFFER_SIZE = 1024;

    void SetUp() override {
        server = std::make_unique<Server>(Visibility::LocalMachine);
    }

    int connect_client() const {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(test_port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock);
            return -1;
        }
        return sock;
    }
};

// Test visibility configurations
TEST_F(ServerTest, ConfigureVisibility_LocalMachine) {
    ASSERT_TRUE(server->configure_visibility(Visibility::LocalMachine));
    EXPECT_FALSE(server->is_running());
}

TEST_F(ServerTest, ConfigureVisibility_Global) {
    ASSERT_TRUE(server->configure_visibility(Visibility::Global));
}

TEST_F(ServerTest, ConfigureVisibility_Invalid) {
    EXPECT_FALSE(server->configure_visibility(Visibility::Network, "invalid_ip"));
}

// Test starting and stopping the server
TEST_F(ServerTest, StartServer) {
    EXPECT_TRUE(server->start_server(test_port));
    EXPECT_TRUE(server->is_running());
}

TEST_F(ServerTest, StopServer) {
    server->start_server(test_port);
    server->stop_server();
    EXPECT_FALSE(server->is_running());
}

TEST_F(ServerTest, StartServerBindFailure) {
    Server another_server(Visibility::LocalMachine);
    another_server.start_server(test_port);

    EXPECT_FALSE(server->start_server(test_port));
}

// Test client connection handling
TEST_F(ServerTest, AcceptClientConnection) {
    server->start_server(test_port);

    std::future<int> client_socket = std::async([this]() { return connect_client(); });

    ASSERT_NE(client_socket.get(), -1);
    server->stop_server();
}

TEST_F(ServerTest, ClientDisconnect) {
    server->start_server(test_port);

    int client_socket = connect_client();
    ASSERT_NE(client_socket, -1);
    close(client_socket);  // Client disconnects

    EXPECT_NO_THROW(server->stop_server());
}

// Test data handling
TEST_F(ServerTest, ClientSendData) {
    server->start_server(test_port);

    int client_socket = connect_client();
    ASSERT_NE(client_socket, -1);

    std::string message = "Hello from client!";
    send(client_socket, message.c_str(), message.size(), 0);

    std::string buffer(MAX_TEST_BUFFER_SIZE, '\0');
    ssize_t bytes_received = recv(client_socket, buffer.data(), buffer.size(), 0);
    ASSERT_GT(bytes_received, 0);

    buffer.resize(bytes_received);
    EXPECT_EQ(buffer, message);

    close(client_socket);
    server->stop_server();
}

TEST_F(ServerTest, ClientAbruptDisconnect) {
    server->start_server(test_port);

    int client_socket = connect_client();
    ASSERT_NE(client_socket, -1);

    std::string message = "Data before disconnect";
    send(client_socket, message.c_str(), message.size(), 0);
    close(client_socket);  // Abrupt disconnect

    EXPECT_NO_THROW(server->stop_server());
}

// Test buffer overflow handling
TEST_F(ServerTest, BufferOverflowHandling) {
    server->start_server(test_port);

    int client_socket = connect_client();
    ASSERT_NE(client_socket, -1);

    std::string long_message(MAX_TEST_BUFFER_SIZE + 1, 'x');
    send(client_socket, long_message.c_str(), long_message.size(), 0);

    std::string buffer(MAX_TEST_BUFFER_SIZE, '\0');
    ssize_t bytes_received = recv(client_socket, buffer.data(), buffer.size(), 0);
    ASSERT_GT(bytes_received, 0);
    EXPECT_LE(bytes_received, MAX_TEST_BUFFER_SIZE);

    close(client_socket);
    server->stop_server();
}

// Test multithreading with concurrent clients
TEST_F(ServerTest, ConcurrentClients) {
    server->start_server(test_port);

    std::vector<int> client_sockets;
    for (int i = 0; i < 5; ++i) {
        int sock = connect_client();
        ASSERT_NE(sock, -1);
        client_sockets.push_back(sock);
    }

    for (int sock : client_sockets) {
        close(sock);
    }

    server->stop_server();
}

TEST_F(ServerTest, StopServerBeforeStart) {
    EXPECT_NO_THROW(server->stop_server());
}

// Test stopping server while clients are connected
TEST_F(ServerTest, StopServerWithClientsConnected) {
    server->start_server(test_port);

    int client_socket1 = connect_client();
    int client_socket2 = connect_client();
    ASSERT_NE(client_socket1, -1);
    ASSERT_NE(client_socket2, -1);

    server->stop_server();

    std::string buffer(MAX_TEST_BUFFER_SIZE, '\0');
    EXPECT_EQ(recv(client_socket1, buffer.data(), buffer.size(), 0), -1);
    EXPECT_EQ(recv(client_socket2, buffer.data(), buffer.size(), 0), -1);

    close(client_socket1);
    close(client_socket2);
}
