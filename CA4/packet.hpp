#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>
#include <cstring>
#include <vector>

// Define flags based on project requirements
const uint8_t FLAG_DATA = 0x01;
const uint8_t FLAG_ACK  = 0x02;
const uint8_t FLAG_FIN  = 0x04;

const size_t MAX_PAYLOAD_SIZE = 1024;

// Packet header structure with no padding
struct PacketHeader {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t length;
    uint16_t checksum;
    uint8_t flags;
} __attribute__((packed));

// Complete packet structure
struct Packet {
    PacketHeader header;
    char payload[MAX_PAYLOAD_SIZE];
};

// Simple checksum calculation (16-bit one's complement sum)
inline uint16_t calculate_checksum(const Packet* pkt) {
    uint32_t sum = 0;
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(pkt);
    // Calculate total size: header size + payload size
    size_t total_size = sizeof(PacketHeader) + pkt->header.length;
    
    // Sum 16-bit words
    for (size_t i = 0; i < total_size / 2; ++i) {
        // Skip the checksum field itself (offset 10 bytes in header -> index 5)
        if (i == 5) continue; 
        sum += ptr[i];
    }
    
    // Add leftover byte if total_size is odd
    if (total_size % 2 != 0) {
        const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(pkt);
        uint16_t last_byte = byte_ptr[total_size - 1];
        sum += (last_byte << 8); 
    }
    
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}

// Serialize packet to a byte buffer for network transmission
inline std::vector<char> serialize(const Packet& pkt) {
    size_t total_size = sizeof(PacketHeader) + pkt.header.length;
    std::vector<char> buffer(total_size);
    std::memcpy(buffer.data(), &pkt, total_size);
    return buffer;
}

// Deserialize byte buffer back to a packet structure
inline Packet deserialize(const char* buffer, size_t size) {
    Packet pkt;
    std::memset(&pkt, 0, sizeof(Packet));
    if (size >= sizeof(PacketHeader)) {
        std::memcpy(&pkt, buffer, size);
    }
    return pkt;
}

#endif // PACKET_HPP