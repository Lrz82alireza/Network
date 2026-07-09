#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "packet.hpp"

const int TIMEOUT_SEC = 1;
const int TIMEOUT_USEC = 0;

int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error: Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Set receive timeout for Stop-and-Wait
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = TIMEOUT_USEC;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "Error: Setting socket timeout failed." << std::endl;
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void send_file(const std::string& ip, int port, const std::string& filename) {
    int sockfd = create_udp_socket();
    
    struct sockaddr_in receiver_addr;
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &receiver_addr.sin_addr);

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        close(sockfd);
        return;
    }

    uint32_t current_seq = 0;
    int total_packets_sent = 0;
    int total_retransmissions = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();

    while (!file.eof()) {
        Packet pkt;
        std::memset(&pkt, 0, sizeof(Packet));
        
        file.read(pkt.payload, MAX_PAYLOAD_SIZE);
        size_t bytes_read = file.gcount();
        
        if (bytes_read == 0) break;

        pkt.header.seq_num = current_seq;
        pkt.header.ack_num = 0;
        pkt.header.length = static_cast<uint16_t>(bytes_read);
        pkt.header.flags = FLAG_DATA;
        pkt.header.checksum = calculate_checksum(&pkt);
        
        std::vector<char> buffer = serialize(pkt);
        bool ack_received = false;

        while (!ack_received) {
            // Send the packet
            sendto(sockfd, buffer.data(), buffer.size(), 0,
                   (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
            total_packets_sent++;

            // Wait for ACK
            Packet ack_pkt;
            std::memset(&ack_pkt, 0, sizeof(Packet));
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            
            int n = recvfrom(sockfd, &ack_pkt, sizeof(Packet), 0,
                             (struct sockaddr*)&from_addr, &from_len);
            
            if (n > 0) {
                // Verify checksum and ACK number
                uint16_t received_checksum = ack_pkt.header.checksum;
                ack_pkt.header.checksum = 0; 
                uint16_t calculated_checksum = calculate_checksum(&ack_pkt);

                if (calculated_checksum == received_checksum && 
                    (ack_pkt.header.flags & FLAG_ACK) && 
                    ack_pkt.header.ack_num == current_seq) {
                        
                    std::cout << "Ack Recieved Nigga " << current_seq << std::endl;
                    ack_received = true;
                    current_seq++;
                }
            } else {
                // Timeout occurred
                std::cout << "Timeout! Retransmitting sequence " << current_seq << std::endl;
                total_retransmissions++;
            }
        }
    }
    
    // Send FIN packet
    Packet fin_pkt;
    std::memset(&fin_pkt, 0, sizeof(Packet));
    fin_pkt.header.seq_num = current_seq;
    fin_pkt.header.flags = FLAG_FIN;
    fin_pkt.header.checksum = calculate_checksum(&fin_pkt);
    std::vector<char> fin_buffer = serialize(fin_pkt);
    sendto(sockfd, fin_buffer.data(), fin_buffer.size(), 0,
           (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "--- Transfer Statistics ---" << std::endl;
    std::cout << "Total time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Total packets sent: " << total_packets_sent << std::endl;
    std::cout << "Total retransmissions: " << total_retransmissions << std::endl;

    file.close();
    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <Receiver IP> <Receiver Port> <Input File>" << std::endl;
        return 1;
    }
    
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    std::string input_file = argv[3];
    
    send_file(ip, port, input_file);
    return 0;
}