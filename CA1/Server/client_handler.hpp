#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include "client_info.hpp"
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <filesystem>
#include <fstream>
#include <unistd.h>

class ClientHandler {
private:
    // Helper function to send a message to a specific socket
    void sendMessage(int socket_fd, const std::string& message);

    // Command handlers
    void handleUsersCommand(const ClientInfo& sender, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex);
    void handleMsgCommand(const ClientInfo& sender, const std::vector<std::string>& tokens, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex);
    void handlePmCommand(const ClientInfo& sender, const std::vector<std::string>& tokens, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex);
    void handleListCommand(const ClientInfo& sender);
    void handlePutCommand(const ClientInfo& sender, const std::vector<std::string>& tokens);
    void handleGetCommand(const ClientInfo& sender, const std::vector<std::string>& tokens);
    void handleHelpCommand(const ClientInfo& sender);

    const std::string server_storage_path = "server_storage/";

    // Helper function to split string by space
    std::vector<std::string> splitMessage(const std::string& message);

public:
    // Main entry point for processing incoming messages
    ClientHandler();
    void processMessage(const std::string& message, ClientInfo& sender, std::map<int, ClientInfo>& clients, std::mutex& clients_mutex);
};

#endif // CLIENT_HANDLER_HPP
