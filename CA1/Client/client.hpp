#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <atomic>
#include <filesystem>
#include <fstream>


class Client {
private:
    int client_socket;
    int server_port;
    std::string server_ip;
    struct sockaddr_in server_addr;

    std::atomic<bool> is_uploading{false};
    std::atomic<bool> is_downloading{false};
    std::atomic<long> expected_filesize{0};
    std::string current_filename;

    const std::string client_storage_path = "client_storage/";

    void setupSocket();
    void connectToServer();
    
    void receiveMessages();
    void sendMessages();

    std::vector<std::string> splitCommand(const std::string& command);
    void handlePutCommand(const std::string& filename);
    void handleGetCommand(const std::string& filename);

public:
    Client(std::string ip, int port);
    ~Client();

    void start();
};

#endif // CLIENT_HPP
