#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <thread>
#include <chrono>
#include "packet.hpp"
#include "logger.hpp"

// Default simulation parameters
float loss_prob = 0.0f;
int delay_ms = 0;
Logger logger;

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
    // Simulate Delay before sending ACK
    if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    Packet ack_pkt;
    std::memset(&ack_pkt, 0, sizeof(Packet));
    ack_pkt.header.ack_num = ack_num;
    ack_pkt.header.flags = FLAG_ACK;
    ack_pkt.header.checksum = calculate_checksum(&ack_pkt);
    
    std::vector<char> buffer = serialize(ack_pkt);
    sendto(sockfd, buffer.data(), buffer.size(), 0, 
           (const struct sockaddr*)&client_addr, client_len);
           
    logger.log("Sent cumulative ACK for next expected seq: " + std::to_string(ack_num));
}

void parse_arguments(int argc, char* argv[]) {
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--loss" && i + 1 < argc) {
            loss_prob = std::stof(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            delay_ms = std::stoi(argv[++i]);
        }
    }
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

    // Random number generator setup for Loss simulation
    std::mt19937 gen(time(0));
    std::uniform_real_distribution<> dis(0.0, 1.0);

    logger.log("Listening on port " + std::to_string(port) + "...");
    logger.log("Simulation Params -> Loss: " + std::to_string(loss_prob) + ", Delay: " + std::to_string(delay_ms) + "ms");

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
                logger.log("Corrupted packet dropped.");
                continue; 
            }

            if (pkt.header.flags & FLAG_FIN) {
                logger.log("FIN packet received. Transfer complete.");
                receiving = false;
                continue;
            }

            if (pkt.header.flags & FLAG_DATA) {
                // Simulate Packet Loss
                if (dis(gen) < loss_prob) {
                    logger.log("SIMULATED LOSS: Dropped packet seq " + std::to_string(pkt.header.seq_num));
                    continue; // Drop the packet (do nothing)
                }

                logger.log("Received DATA packet, seq: " + std::to_string(pkt.header.seq_num));
                
                if (pkt.header.seq_num == expected_seq) {
                    file.write(pkt.payload, pkt.header.length);
                    expected_seq++;
                }
                // Send ACK for the *expected* seq (Cumulative), whether it was in-order or out-of-order
                send_ack(sockfd, client_addr, client_len, expected_seq);
            }
        }
    }
    
    file.close();
    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <Port> <Output File> [--loss <float>] [--delay <ms>]" << std::endl;
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    std::string output_file = argv[2];
    
    parse_arguments(argc, argv);
    receive_file(port, output_file);
    
    return 0;
}