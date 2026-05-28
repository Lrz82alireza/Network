#ifndef SERVER_HPP
#define SERVER_HPP

#include "client_info.hpp"
#include "client_handler.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>

class Server {
private:
    int server_fd;
    int port;
    struct sockaddr_in server_addr;
    
    std::map<int, ClientInfo> clients;
    std::mutex clients_mutex;

    ClientHandler client_handler;

    // توابع کمکی
    void setupSocket();
    void bindSocket();
    void startListening(int Client_limit);
    void acceptClients();
    void handleClient(int client_socket);
    void removeClient(int client_socket);

public:
    Server(int port);
    ~Server();

    void start();
};

#endif // SERVER_HPP
