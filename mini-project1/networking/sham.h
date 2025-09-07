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

#define MAX_DATA_SIZE 1024

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

typedef struct sham_header {
    uint32_t seq_num; // Sequence Number
    uint32_t ack_num; // Acknowledgment Number
    uint16_t flags; // Control flags (SYN, ACK, FIN)
    uint16_t window_size; // Flow control window size
} sham_header;

// Packet struct Abstraction with TCP headers
typedef struct Packet {
    sham_header header;
    char data[MAX_DATA_SIZE];
} Packet;

// Packet information (Packet, Timesent, packet length)
typedef struct sent_packet_info {
    Packet packet;
    struct timeval time_sent;
    size_t packet_len;
} sent_packet_info;

// Functionalities
void packetDebugPrint(Packet * packet, const char * direction); // debug fn for Packet

#endif
