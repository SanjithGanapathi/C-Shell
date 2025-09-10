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
#include<stdarg.h> // For va_list

// --- Global Log File ---
// Declared as static so each .c file gets its own instance, preventing linker errors.
static FILE *log_file = NULL;

// --- S.H.A.M. Definitions ---
#define SYN 0x1
#define ACK 0x2
#define FIN 0x4
#define FILENAME_FLAG 0x8 

#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 10   
#define RTO_MS 500       
#define MAX_BUFFER_SIZE 2048
#define MAX_DATA_SIZE 2048 // Kept for chat buffer compatibility

// Use __attribute__((packed)) to prevent compiler padding
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

typedef struct sent_packet_info {
    Packet packet;
    struct timeval time_sent;
    size_t packet_len;
    bool acked;
} sent_packet_info;

typedef struct received_packet_info {
    Packet packet;
    size_t len;
    bool received;
} received_packet_info;

// --- MODULAR LOGGING FUNCTION ---
// Defined as static inline so it can be used by any file including this header.
static inline void log_event(const char *format, ...) {
    // If logging is not enabled (log_file is NULL), do nothing.
    if (log_file == NULL) {
        return;
    }

    // --- Timestamp Generation ---
    char time_buffer[30];
    struct timeval tv;
    time_t curtime;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    // Print the timestamp prefix to the log file
    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, tv.tv_usec);

    // --- Message Printing ---
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file); // Ensure the log is written to the file immediately
}


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


