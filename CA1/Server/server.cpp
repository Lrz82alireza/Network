#include "server.hpp"

Server::Server(int port) : port(port), server_fd(-1) {
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(port);
}

Server::~Server() {
    if (server_fd != -1) {
        close(server_fd);
    }
}

void Server::setupSocket() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Error: Could not create socket!" << std::endl;
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed!" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Server::bindSocket() {
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error: Bind failed on port " << port << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Server::startListening(int Client_limit) {
    if (listen(server_fd, Client_limit) < 0) {
        std::cerr << "Error: Listen failed!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "[+] Server is listening on port " << port << "..." << std::endl;
}

void Server::removeClient(int client_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(client_socket);
    close(client_socket);
    std::cout << "[-] Client " << client_socket << " disconnected." << std::endl;
}

void Server::handleClient(int client_socket) {
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        // Client disconnected or error occurred
        if (bytes_received <= 0) {
            removeClient(client_socket);
            // TODO: Report the client has disconnected
            break;
        }

        std::string message(buffer);
        
        // Remove trailing newline characters if any
        if (!message.empty() && message[message.length()-1] == '\n') {
            message.erase(message.length()-1);
        }

        // Send the message to the Handler
        // We pass the ClientInfo struct by reference from the map
        client_handler.processMessage(message, clients[client_socket], clients, clients_mutex);
    }
}

void Server::acceptClients() {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            std::cerr << "Error: Accept failed!" << std::endl;
            continue;
        }

        std::cout << "[+] New client connected with FD: " << client_socket << std::endl;

        // Create a new ClientInfo object and add it to the map safely
        ClientInfo new_client;
        new_client.socket_fd = client_socket;
        new_client.username = "User_" + std::to_string(client_socket); // Default username
        new_client.is_registered = false;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[client_socket] = new_client;
        }

        // Spawn a new thread to handle this client
        std::thread client_thread(&Server::handleClient, this, client_socket);
        client_thread.detach();
    }
}

void Server::start() {
    setupSocket();
    bindSocket();
    startListening(10);
    acceptClients(); 
}


int main() {
    Server myServer(8080);
    myServer.start();
    return 0;
}