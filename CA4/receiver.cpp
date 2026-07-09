#include <iostream>
#include <fstream>
#include <string>
#include "packet.hpp"

void receive_file(const std::string& filename) {
    // Open file in binary mode for writing
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create file " << filename << std::endl;
        return;
    }

    // TODO: Setup UDP socket and listen for incoming packets (Phase 1)
    
    // Simulated packet reception logic
    bool receiving = true;
    while (receiving) {
        // TODO: Receive buffer from socket and deserialize
        // Packet pkt = deserialize(buffer, bytes_received);
        
        // TODO: Verify checksum and sequence number
        
        // Write payload to file
        // file.write(pkt.payload, pkt.header.length);
        
        // For now, break loop to prevent infinite execution in Phase 0
        break; 
    }
    
    file.close();
    std::cout << "File writing completed." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <Port> <Output File>" << std::endl;
        return 1;
    }
    
    std::string output_file = argv[2];
    receive_file(output_file);
    
    return 0;
}