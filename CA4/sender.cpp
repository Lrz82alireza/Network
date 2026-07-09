#include <iostream>
#include <fstream>
#include <string>
#include "packet.hpp"

void send_file(const std::string& filename) {
    // Open file in binary mode
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return;
    }

    uint32_t current_seq = 0;
    
    // Read file in chunks and create packets
    while (!file.eof()) {
        Packet pkt;
        std::memset(&pkt, 0, sizeof(Packet));
        
        file.read(pkt.payload, MAX_PAYLOAD_SIZE);
        size_t bytes_read = file.gcount();
        
        if (bytes_read > 0) {
            pkt.header.seq_num = current_seq;
            pkt.header.ack_num = 0;
            pkt.header.length = static_cast<uint16_t>(bytes_read);
            pkt.header.flags = FLAG_DATA;
            pkt.header.checksum = calculate_checksum(&pkt);
            
            // TODO: Serialize and send via UDP socket (Phase 1)
            
            current_seq++;
        }
    }
    
    file.close();
    std::cout << "File reading completed." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <Receiver IP> <Receiver Port> <Input File>" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[3];
    send_file(input_file);
    
    return 0;
}