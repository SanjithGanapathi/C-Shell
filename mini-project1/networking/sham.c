#include "sham.h"

void packetDebugPrint(Packet * packet, const char* direction) {
    printf("%s packet: | ", direction);
    printf("Seq: %u | ", ntohl(packet->header.seq_num));
    printf("Ack: %u | ", ntohl(packet->header.ack_num));
    printf("Flags: ");
    if (ntohs(packet->header.flags) & SYN) printf("SYN ");
    if (ntohs(packet->header.flags) & ACK) printf("ACK ");
    if (ntohs(packet->header.flags) & FIN) printf("FIN ");
    printf("| Win: %u |\n", ntohs(packet->header.window_size));
}
