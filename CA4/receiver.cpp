#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "packet.hpp"
#include "logger.hpp"

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
           
    log_msg("Sent cumulative ACK for next expected seq: " + std::to_string(ack_num));
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

    log_msg("Listening on port " + std::to_string(port) + "...");

    while (receiving) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int n = recvfrom(sockfd, buffer, sizeof(Packet), 0, 
                         (struct sockaddr*)&client_addr, &client_len);
        
        if (n > 0) {
            Packet pkt = deserialize(buffer, n);
            
            uint16_t received_checksum = pkt.header.checksum;
            pkt.header.checksum = 0;
            uint16_t calculated_checksum = calculate_checksum(&pkt);
            
            if (calculated_checksum != received_checksum) {
                log_msg("Corrupted packet dropped.");
                continue; 
            }

            if (pkt.header.flags & FLAG_FIN) {
                log_msg("FIN packet received. Transfer complete.");
                receiving = false;
                continue;
            }

            if (pkt.header.flags & FLAG_DATA) {
                log_msg("Received DATA packet, seq: " + std::to_string(pkt.header.seq_num));
                
                if (pkt.header.seq_num == expected_seq) {
                    // In-order packet
                    file.write(pkt.payload, pkt.header.length);
                    expected_seq++;
                    send_ack(sockfd, client_addr, client_len, expected_seq);
                } else {
                    // Out-of-order or duplicate packet, send ACK for the expected seq
                    send_ack(sockfd, client_addr, client_len, expected_seq);
                }
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