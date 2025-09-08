#ifndef SHAM_H_
#define SHAM_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include<time.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<stdint.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<errno.h>
#include<sys/time.h> // For gettimeofday

// Define S.H.A.M. packet flags
#define SYN 0x1
#define ACK 0x2
#define FIN 0x4
#define FILENAME_FLAG 0x8 // Flag to identify filename packet

// Define Global Size
#define PAYLOAD_SIZE 1024 // Fixed size of data chunks
#define WINDOW_SIZE 10    // Sender's fixed sliding window size (in packets)
#define RTO_MS 500        // Retransmission Timeout in milliseconds
#define MAX_BUFFER_SIZE 2048
#define MAX_DATA_SIZE 2048

// Use __attribute__((packed)) to prevent compiler padding which corrupts network packets
typedef struct sham_header {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t flags;
    uint16_t window_size;
} __attribute__((packed)) sham_header;

typedef struct Packet {
    sham_header header;
    char data[PAYLOAD_SIZE];
} __attribute__((packed)) Packet;

// Packet information for sender's window buffer
typedef struct sent_packet_info {
    Packet packet;
    struct timeval time_sent;
    size_t packet_len;
    bool acked;
} sent_packet_info;

// Packet information for receiver's out-of-order buffer
typedef struct received_packet_info {
    Packet packet;
    size_t len;
    bool received;
} received_packet_info;


// A helper function to print packet details for debugging
void packetDebugPrint(Packet *packet, const char *direction) {
    uint16_t flags = ntohs(packet->header.flags);
    printf("%s packet: | ", direction);
    printf("Seq: %u | ", ntohl(packet->header.seq_num));
    printf("Ack: %u | ", ntohl(packet->header.ack_num));
    printf("Flags: ");
    if (flags & SYN) printf("SYN ");
    if (flags & ACK) printf("ACK ");
    if (flags & FIN) printf("FIN ");
    if (flags & FILENAME_FLAG) printf("FILENAME ");
    printf("| Win: %u |\n", ntohs(packet->header.window_size));
}

#endif


