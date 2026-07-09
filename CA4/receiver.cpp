#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "packet.hpp"

int setup_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error: Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error: Bind failed." << std::endl;
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void send_ack(int sockfd, const struct sockaddr_in& client_addr, socklen_t client_len, uint32_t ack_num) {
    Packet ack_pkt;
    std::memset(&ack_pkt, 0, sizeof(Packet));
    ack_pkt.header.ack_num = ack_num;
    ack_pkt.header.flags = FLAG_ACK;
    ack_pkt.header.checksum = calculate_checksum(&ack_pkt);
    
    std::vector<char> buffer = serialize(ack_pkt);
    sendto(sockfd, buffer.data(), buffer.size(), 0, 
           (const struct sockaddr*)&client_addr, client_len);
}

void receive_file(int port, const std::string& filename) {
    int sockfd = setup_server_socket(port);
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create file " << filename << std::endl;
        close(sockfd);
        return;
    }

    uint32_t expected_seq = 0;
    bool receiving = true;
    char buffer[sizeof(Packet)];

    std::cout << "Listening on port " << port << "..." << std::endl;

    while (receiving) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int n = recvfrom(sockfd, buffer, sizeof(Packet), 0, 
                         (struct sockaddr*)&client_addr, &client_len);
        
        if (n > 0) {
            Packet pkt = deserialize(buffer, n);
            
            // Checksum verification
            uint16_t received_checksum = pkt.header.checksum;
            pkt.header.checksum = 0;
            uint16_t calculated_checksum = calculate_checksum(&pkt);
            
            if (calculated_checksum != received_checksum) {
                std::cout << "Corrupted packet dropped." << std::endl;
                continue; 
            }

            // Handle FIN packet
            if (pkt.header.flags & FLAG_FIN) {
                std::cout << "FIN packet received. Transfer complete." << std::endl;
                receiving = false;
                continue;
            }

            // Handle DATA packet
            if (pkt.header.flags & FLAG_DATA) {
                if (pkt.header.seq_num == expected_seq) {
                    // In-order packet, write to file
                    file.write(pkt.payload, pkt.header.length);
                    send_ack(sockfd, client_addr, client_len, pkt.header.seq_num);
                    expected_seq++;
                } else if (pkt.header.seq_num < expected_seq) {
                    // Duplicate packet (ACK was likely lost), resend ACK
                    send_ack(sockfd, client_addr, client_len, pkt.header.seq_num);
                }
                // Out-of-order packets (seq > expected) are dropped in Stop-and-Wait
            }
        }
    }
    
    file.close();
    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <Port> <Output File>" << std::endl;
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    std::string output_file = argv[2];
    
    receive_file(port, output_file);
    return 0;
}