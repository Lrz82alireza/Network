#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include "packet.hpp"
#include "logger.hpp"

const uint32_t WINDOW_SIZE = 5;
const int TIMEOUT_SEC = 1;
const int TIMEOUT_USEC = 0;

int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error: Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

std::vector<Packet> prepare_packets(const std::string& filename) {
    std::vector<Packet> packets;
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return packets;
    }

    uint32_t current_seq = 0;
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
        
        packets.push_back(pkt);
        current_seq++;
    }
    file.close();
    return packets;
}

void send_file(const std::string& ip, int port, const std::string& filename) {
    int sockfd = create_udp_socket();
    struct sockaddr_in receiver_addr;
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &receiver_addr.sin_addr);

    std::vector<Packet> packets = prepare_packets(filename);
    if (packets.empty()) {
        close(sockfd);
        return;
    }

    uint32_t base = 0;
    uint32_t next_seq = 0;
    uint32_t total_packets = packets.size();
    int total_retransmissions = 0;

    log_msg("Starting Sliding Window transmission. Total packets: " + std::to_string(total_packets));

    while (base < total_packets) {
        // Send packets within the window
        while (next_seq < base + WINDOW_SIZE && next_seq < total_packets) {
            std::vector<char> buffer = serialize(packets[next_seq]);
            sendto(sockfd, buffer.data(), buffer.size(), 0,
                   (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
            
            log_msg("Sent DATA packet, seq: " + std::to_string(next_seq));
            next_seq++;
        }

        // Wait for ACKs using select
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = TIMEOUT_USEC;

        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret > 0) {
            Packet ack_pkt;
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            int n = recvfrom(sockfd, &ack_pkt, sizeof(Packet), 0,
                             (struct sockaddr*)&from_addr, &from_len);
            
            if (n > 0) {
                uint16_t received_checksum = ack_pkt.header.checksum;
                ack_pkt.header.checksum = 0; 
                uint16_t calculated_checksum = calculate_checksum(&ack_pkt);

                if (calculated_checksum == received_checksum && (ack_pkt.header.flags & FLAG_ACK)) {
                    log_msg("Received ACK for expected seq: " + std::to_string(ack_pkt.header.ack_num));
                    
                    // Cumulative ACK: Slide window forward if ACK is valid
                    if (ack_pkt.header.ack_num > base) {
                        base = ack_pkt.header.ack_num;
                    }
                }
            }
        } else if (ret == 0) {
            // Timeout: Go-Back-N retransmission
            log_msg("Timeout occurred! Retransmitting window starting from seq: " + std::to_string(base));
            next_seq = base; // Reset next_seq to base to resend unACKed packets
            total_retransmissions++;
        }
    }

    // Send FIN packet
    Packet fin_pkt;
    std::memset(&fin_pkt, 0, sizeof(Packet));
    fin_pkt.header.seq_num = total_packets;
    fin_pkt.header.flags = FLAG_FIN;
    fin_pkt.header.checksum = calculate_checksum(&fin_pkt);
    std::vector<char> fin_buffer = serialize(fin_pkt);
    
    sendto(sockfd, fin_buffer.data(), fin_buffer.size(), 0,
           (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
    log_msg("Sent FIN packet. Transfer complete.");
    log_msg("Total Retransmission events: " + std::to_string(total_retransmissions));

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