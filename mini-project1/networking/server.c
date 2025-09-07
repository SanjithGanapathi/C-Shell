#include "sham.h"

int main(int argc, char * argv[]) {
    // Argument check
    if(argc < 2) {
        printf("INCORRECT FORMAT: -- Usage: %s, <port> [--chat] [loss_rate]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    bool chatMode = false;
    double loss_rate = 0.0;

    // Set up the chatMode and loss_rate
    for(int i = 2; i<argc; i++) {
        if(!strcmp(argv[i], "--chat")) {
            chatMode = true;
        } else {
            // Convert the char * to float
            loss_rate = atof(argv[i]);
        }
    }

    // Seed the random num generator
    srand(time(NULL));

    // Declaring variables for Socket Creation
    int sockFD;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    Packet send_packet, recv_packet;

    // Creation of socket (IPV4, Datagram(UDP), Protocol Number)
    if((sockFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failure\n");
        exit(EXIT_FAILURE);
    }

    // Address Info
    memset(&server_addr, 0, sizeof(server_addr)); // Allocate memory and initialize it
    server_addr.sin_family = AF_INET; // IPV4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on any interfaces
    server_addr.sin_port = htons(port); // Host(port no) to Network byte order

    // Bind the socket to server address
    if(bind(sockFD, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind Failure\n");
        close(sockFD); // Remove from FD Table
        exit(EXIT_FAILURE);
    }

    printf("\n[STATUS] Server is listening on port %d...\n\n", port);

    // Server Loop
    while(1) {
        // Three Way Handshake
        
        ssize_t bytesReceived = recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &client_len);
        if(bytesReceived < 0) {
            printf("Error in receiving bytes\n");
            continue; // Wait for the next Packets
        }

        // Check and proceed only if a SYN Packet for later stages
        if((ntohs(recv_packet.header.flags) & SYN) && !(ntohs(recv_packet.header.flags) & ACK)) { // Network byte order to host no 
            printf("[HANDSHAKE] Received SYN from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); // IP_Address:Port No (converted from n to h)
//            packetDebugPrint(&recv_packet, "<- RECV");

            // Get the clients Sequence Number from th packet header
            uint32_t client_seq_num_x = ntohl(recv_packet.header.seq_num);

            // Send a SYN-ACK Packet to client
            memset(&send_packet, 0, sizeof(send_packet)); // Allocate Mem and initialize
            uint32_t server_seq_num_y = rand() % 10000; // Generate a Random Seq Num                 
            
            // Encode Header Info into the send_packet
            send_packet.header.seq_num = htonl(server_seq_num_y); // seq_num host to long
            send_packet.header.ack_num = htonl(client_seq_num_x + 1); // ack_num
            send_packet.header.flags = htonl(SYN | ACK); // Both SYN-ACK Flags
            send_packet.header.window_size = htons(5000); // window size ***IMPORTANT***SET IT LATER
            
            // Send to client
            if(sendto(sockFD, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&client_addr, client_len) < 0) {
                printf("Send SYN_ACK Packet Failure\n");
            }

            // ACK Packet from client 
            bytesReceived = recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &client_len);
             if(bytesReceived < 0) {
                printf("recvfrom error during final ACK");
                continue;
            }

            // Validate the ACK Packet
            if((ntohs(recv_packet.header.flags) & ACK) && (ntohl(recv_packet.header.ack_num) == server_seq_num_y + 1)) {

                printf("[HANDSHAKE] Received ACK from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); // IP_Address:Port No (converted from n to h)
  //              packetDebugPrint(&recv_packet, "<- RECV");

                printf("\n [STATUS] Connection Establised %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); // IP_Address:Port No (converted from n to h)
            } else {
                printf("[ERROR] Handshake failed: Invalid final ACK.\n");
            }
        } else {
            printf("[INFO] Received a non-SYN packet. Ignoring.\n");
        }
    }

    close(sockFD);
    return 0;
}









