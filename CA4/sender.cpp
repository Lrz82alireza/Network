#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <algorithm>
#include "packet.hpp"
#include "logger.hpp"

const int TIMEOUT_SEC = 1;
const int TIMEOUT_USEC = 0;
const uint32_t RECEIVER_WINDOW = 50; // Fixed max receiver window

int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error: Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

std::vector<Packet> prepare_packets(const std::string& filename, size_t& out_total_bytes) {
    std::vector<Packet> packets;
    std::ifstream file(filename, std::ios::binary);
    out_total_bytes = 0;
    
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
        out_total_bytes += bytes_read;
        current_seq++;
    }
    file.close();
    return packets;
}

void send_file(const std::string& ip, int port, const std::string& filename) {
    // Initialize Loggers
    Logger logger("Junk/Logs/events.log");
    std::ofstream cwnd_file("Junk/Logs/cwnd.csv", std::ios::trunc);
    if (cwnd_file.is_open()) {
        cwnd_file << "Time(s),Cwnd\n";
    }

    int sockfd = create_udp_socket();
    struct sockaddr_in receiver_addr;
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &receiver_addr.sin_addr);

    size_t total_payload_bytes = 0;
    std::vector<Packet> packets = prepare_packets(filename, total_payload_bytes);
    if (packets.empty()) {
        close(sockfd);
        return;
    }

    uint32_t base = 0;
    uint32_t next_seq = 0;
    uint32_t total_packets = packets.size();
    int total_retransmissions = 0;

    // Congestion Control Variables
    double cwnd = 1.0;
    int ssthresh = 16;
    
    logger.log("Starting transmission. Total packets: " + std::to_string(total_packets));
    auto start_time = std::chrono::high_resolution_clock::now();

    auto record_cwnd = [&](double current_cwnd) {
        if (cwnd_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> t = now - start_time;
            cwnd_file << t.count() << "," << current_cwnd << "\n";
            cwnd_file.flush();
        }
    };
    
    record_cwnd(cwnd);

    while (base < total_packets) {
        // effective_window = min(receiver_window, cwnd)
        uint32_t effective_window = std::min(RECEIVER_WINDOW, static_cast<uint32_t>(cwnd));

        while (next_seq < base + effective_window && next_seq < total_packets) {
            std::vector<char> buffer = serialize(packets[next_seq]);
            sendto(sockfd, buffer.data(), buffer.size(), 0,
                   (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
            
            logger.log("Sent DATA packet, seq: " + std::to_string(next_seq));
            next_seq++;
        }

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
                    logger.log("Received ACK for expected seq: " + std::to_string(ack_pkt.header.ack_num));
                    
                    if (ack_pkt.header.ack_num > base) {
                        base = ack_pkt.header.ack_num;
                        
                        // --- CONGESTION CONTROL UPDATE ---
                        if (cwnd < ssthresh) {
                            // Slow Start: Exponential growth
                            cwnd += 1.0;
                            logger.log("Slow Start -> cwnd increased to " + std::to_string(cwnd));
                        } else {
                            // Congestion Avoidance: Linear growth (approx 1 packet per RTT)
                            cwnd += (1.0 / static_cast<int>(cwnd));
                            logger.log("Congestion Avoidance -> cwnd increased to " + std::to_string(cwnd));
                        }
                        record_cwnd(cwnd);
                    }
                }
            }
        } else if (ret == 0) {
            logger.log("Timeout occurred! Retransmitting window from seq: " + std::to_string(base));
            
            // --- CONGESTION CONTROL: TIMEOUT ---
            ssthresh = std::max(static_cast<int>(cwnd / 2.0), 1);
            cwnd = 1.0;
            logger.log("TIMEOUT! -> ssthresh set to " + std::to_string(ssthresh) + ", cwnd dropped to 1.0");
            record_cwnd(cwnd);

            next_seq = base; 
            total_retransmissions++;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double throughput = (total_payload_bytes) / elapsed.count(); // Bytes per second

    Packet fin_pkt;
    std::memset(&fin_pkt, 0, sizeof(Packet));
    fin_pkt.header.seq_num = total_packets;
    fin_pkt.header.flags = FLAG_FIN;
    fin_pkt.header.checksum = calculate_checksum(&fin_pkt);
    std::vector<char> fin_buffer = serialize(fin_pkt);
    sendto(sockfd, fin_buffer.data(), fin_buffer.size(), 0,
           (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
           
    logger.log("Sent FIN packet. Transfer complete.");
    logger.log("--- STATISTICS ---");
    logger.log("Total Retransmission events: " + std::to_string(total_retransmissions));
    logger.log("Total transfer time: " + std::to_string(elapsed.count()) + " seconds");
    logger.log("Throughput: " + std::to_string(throughput) + " Bytes/sec");

    close(sockfd);
    if (cwnd_file.is_open()) cwnd_file.close();
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