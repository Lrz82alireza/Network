#ifndef CLIENT_INFO_HPP
#define CLIENT_INFO_HPP

#include <string>

struct ClientInfo {
    int socket_fd;
    std::string username;
    bool is_registered; // Flag to check if user has set a username
};

#endif // CLIENT_INFO_HPP
