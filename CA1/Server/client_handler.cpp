#include "client_handler.hpp"
#include <sys/socket.h>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

ClientHandler::ClientHandler() {
    if (!fs::exists(server_storage_path)) {
        fs::create_directory(server_storage_path);
        std::cout << "Created directory: " << server_storage_path << std::endl;
    }
}

void ClientHandler::sendMessage(int socket_fd, const std::string& message) {
    send(socket_fd, message.c_str(), message.length(), 0);
}

std::vector<std::string> ClientHandler::splitMessage(const std::string& message) {
    std::istringstream iss(message);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void ClientHandler::handleUsersCommand(const ClientInfo& sender, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex) {
    std::string response = "--- Online Users ---\n";
    
    // Lock the mutex before reading the map
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (const auto& pair : clients) {
        response += "- " + pair.second.username + " (FD: " + std::to_string(pair.second.socket_fd) + ")\n";
    }
    response += "--------------------\n";
    
    sendMessage(sender.socket_fd, response);
}

void ClientHandler::handleMsgCommand(const ClientInfo& sender, const std::vector<std::string>& tokens, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex) {
    if (tokens.size() < 2) {
        sendMessage(sender.socket_fd, "Server: Usage: MSG <message>");
        return;
    }
    
    // Reconstruct the message from tokens
    std::string chat_message;
    for (size_t i = 1; i < tokens.size(); ++i) {
        chat_message += tokens[i] + " ";
    }
    // Remove trailing space
    chat_message.pop_back();

    std::string formatted_message = "[Public from: " + sender.username + "]: " + chat_message;

    // Lock the clients map and broadcast the message
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& pair : clients) {
        // Send to everyone except the sender
        if (pair.second.socket_fd != sender.socket_fd) {
            sendMessage(pair.second.socket_fd, formatted_message);
        }
    }
}

void ClientHandler::handlePmCommand(const ClientInfo& sender, const std::vector<std::string>& tokens, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex) {
    if (tokens.size() < 3) {
        sendMessage(sender.socket_fd, "Server: Usage: PM <recipient_username> <message>");
        return;
    }

    std::string recipient_username = tokens[1];

    // Check if user is trying to send a message to themselves
    if (recipient_username == sender.username) {
        sendMessage(sender.socket_fd, "Server: You cannot send a private message to yourself.");
        return;
    }

    // Reconstruct the message
    std::string private_message;
    for (size_t i = 2; i < tokens.size(); ++i) {
        private_message += tokens[i] + " ";
    }
    private_message.pop_back();

    std::string formatted_message = "(Private from " + sender.username + "): " + private_message;

    int recipient_socket = -1;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& pair : clients) {
            if (pair.second.username == recipient_username) {
                recipient_socket = pair.second.socket_fd;
                break;
            }
        }
    }

    if (recipient_socket != -1) {
        sendMessage(recipient_socket, formatted_message);
        sendMessage(sender.socket_fd, "Server: Private message sent to " + recipient_username);
    } else {
        sendMessage(sender.socket_fd, "Server: Error: User '" + recipient_username + "' not found.");
    }
}

void ClientHandler::handleListCommand(const ClientInfo& sender) {
    std::string response = "--- Files on Server ---\n";
    bool isEmpty = true;
    
    for (const auto& entry : fs::directory_iterator(server_storage_path)) {
        if (entry.is_regular_file()) {
            response += "- " + entry.path().filename().string() + 
                        " (" + std::to_string(fs::file_size(entry)) + " bytes)\n";
            isEmpty = false;
        }
    }
    
    if (isEmpty) {
        response += "No files available.\n";
    }
    response += "-----------------------";
    
    sendMessage(sender.socket_fd, response);
}

void ClientHandler::handlePutCommand(const ClientInfo& sender, const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        sendMessage(sender.socket_fd, "Server: Usage: PUT <filename> <filesize>");
        return;
    }

    std::string filename = tokens[1];
    long filesize = std::stol(tokens[2]);
    std::string filepath = server_storage_path + filename;
    
    std::ofstream outfile(filepath, std::ios::binary);
    char buffer[1024];
    long total_received = 0;

    sendMessage(sender.socket_fd, "READY_FOR_FILE"); 

    while (total_received < filesize) {
        int bytes_to_read = std::min((long)1024, filesize - total_received);
        int bytes_received = recv(sender.socket_fd, buffer, bytes_to_read, 0);
        
        if (bytes_received <= 0) break; 
        
        outfile.write(buffer, bytes_received);
        total_received += bytes_received;
    }
    
    outfile.close();
    sendMessage(sender.socket_fd, "Server: File '" + filename + "' uploaded successfully.");
}

void ClientHandler::handleGetCommand(const ClientInfo& sender, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        sendMessage(sender.socket_fd, "Server: Usage: GET <filename>");
        return;
    }

    std::string filename = tokens[1];
    std::string filepath = server_storage_path + filename;

    if (!fs::exists(filepath)) {
        sendMessage(sender.socket_fd, "Server: Error: File not found.");
        return;
    }

    long filesize = fs::file_size(filepath);
    
    // ۱. ارسال حجم فایل به کلاینت
    sendMessage(sender.socket_fd, "FILE_SIZE " + std::to_string(filesize));

    // ۲. منتظر ماندن برای تاییدیه (ACK) از سمت کلاینت (هندشیک)
    char ack_buffer[256];
    int bytes_received = recv(sender.socket_fd, ack_buffer, sizeof(ack_buffer) - 1, 0);
    if (bytes_received <= 0) {
        return;
    }
    
    std::ifstream infile(filepath, std::ios::binary);
    char buffer[1024];
    
    while (infile.read(buffer, sizeof(buffer)) || infile.gcount() > 0) {
        send(sender.socket_fd, buffer, infile.gcount(), 0);
    }
    
    infile.close();
}

void ClientHandler::handleHelpCommand(const ClientInfo& sender) {
    std::string help_text = 
        "\n=== Available Commands ===\n"
        "USERS                 : Show a list of all connected users.\n"
        "MSG <message>         : Send a broadcast message to all users.\n"
        "PM <user_id> <message>: Send a private message to a specific user.\n"
        "LIST                  : Show all files available on the server.\n"
        "PUT <filename>        : Upload a file to the server.\n"
        "GET <filename>        : Download a file from the server.\n"
        "QUIT                  : Disconnect from the server.\n"
        "HELP                  : Show this help message.\n"
        "==========================\n";

    send(sender.socket_fd, help_text.c_str(), help_text.length(), 0);
}


void ClientHandler::processMessage(const std::string& message, ClientInfo& sender, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex) {
    std::vector<std::string> tokens = splitMessage(message);
    if (tokens.empty()) return;
    std::string command = tokens[0];

    if (command == "USERS") handleUsersCommand(sender, clients, clients_mutex);
    else if (command == "MSG") handleMsgCommand(sender, tokens, clients, clients_mutex);
    else if (command == "PM") handlePmCommand(sender, tokens, clients, clients_mutex);
    else if (command == "LIST") handleListCommand(sender);
    else if (command == "PUT") handlePutCommand(sender, tokens);
    else if (command == "GET") handleGetCommand(sender, tokens);
    else if (command == "HELP") handleHelpCommand(sender);
    else sendMessage(sender.socket_fd, "Server: Unknown command.");
}
