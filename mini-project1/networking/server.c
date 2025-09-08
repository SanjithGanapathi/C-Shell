#include "sham.h"

void handle_connection(int sockFD, struct sockaddr_in client_addr, socklen_t client_len, Packet initial_packet, bool chatMode, double loss_rate);

int main(int argc, char * argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <port> [--chat] [loss_rate]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    bool chatMode = false;
    double loss_rate = 0.0;
    for(int i = 2; i < argc; i++) {
        if(!strcmp(argv[i], "--chat")) chatMode = true;
        else loss_rate = atof(argv[i]);
    }

    srand(time(NULL));
    int sockFD;
    struct sockaddr_in server_addr;
    
    if((sockFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failure");
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if(bind(sockFD, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind Failure");
        close(sockFD);
        exit(EXIT_FAILURE);
    }
    
    printf("[STATUS] Server is listening on port %d...\n", port);
    
    while(1) {
        printf("\n---------------------------------------------\n");
        printf("[INFO] Waiting for a new client connection...\n");
        Packet recv_packet;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        ssize_t bytesReceived = recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &client_len);

        if (bytesReceived > 0 && (ntohs(recv_packet.header.flags) & SYN) && !(ntohs(recv_packet.header.flags) & ACK)) {
             printf("[HANDSHAKE] Received SYN from new client %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            // Handle this client from start to finish (Iterative Model)
            handle_connection(sockFD, client_addr, client_len, recv_packet, chatMode, loss_rate);
        } else {
            printf("[WARN] Received non-SYN packet. Ignoring.\n");
        }
    }
    close(sockFD);
    return 0;
}

void handle_connection(int sockFD, struct sockaddr_in client_addr, socklen_t client_len, Packet recv_packet, bool chatMode, double loss_rate) {
    Packet send_packet;
    
    // --- Handshake continued ---
    uint32_t client_seq_num_x = ntohl(recv_packet.header.seq_num);
    uint32_t server_seq_num_y = rand() % 10000;
    
    memset(&send_packet, 0, sizeof(send_packet));
    send_packet.header.seq_num = htonl(server_seq_num_y);
    send_packet.header.ack_num = htonl(client_seq_num_x + 1);
    send_packet.header.flags = htons(SYN | ACK);
    send_packet.header.window_size = htons(MAX_BUFFER_SIZE);
    sendto(sockFD, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&client_addr, client_len);
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockFD, &readfds);
    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    int activity = select(sockFD + 1, &readfds, NULL, NULL, &timeout);

    if (activity <= 0) {
        printf("[ERROR] Handshake failed: Did not receive final ACK from %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        return; 
    }
    
    ssize_t bytesReceived = recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &client_len);
    if ((ntohs(recv_packet.header.flags) & ACK) && (ntohl(recv_packet.header.ack_num) == server_seq_num_y + 1)) {
        printf("[STATUS] Connection Established with %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    } else {
        printf("[ERROR] Handshake failed for %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        return;
    }

    // --- Application Logic ---
    if (chatMode) {
        printf("[INFO] Chat mode active. Type '/quit' to exit.\n> ");
        fflush(stdout);

        bool keep_chatting = true;
        uint32_t current_seq_num = server_seq_num_y + 1;

        while(keep_chatting) {
            FD_ZERO(&readfds);
            FD_SET(sockFD, &readfds);
            FD_SET(STDIN_FILENO, &readfds);

            int max_fd = (STDIN_FILENO > sockFD) ? STDIN_FILENO : sockFD;
            activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

            if(activity < 0 && errno != EINTR) { perror("select error"); break; }

            // Check for server keyboard input
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                char input_buffer[PAYLOAD_SIZE];
                if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
                    if (strncmp(input_buffer, "/quit", 5) == 0) {
                        printf("[INFO] Server initiated quit. Closing connection...\n");
                        memset(&send_packet, 0, sizeof(send_packet));
                        send_packet.header.seq_num = htonl(current_seq_num);
                        send_packet.header.flags = htons(FIN);
                        sendto(sockFD, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&client_addr, client_len);
                        keep_chatting = false; 
                        continue;
                    }
                    memset(&send_packet, 0, sizeof(send_packet));
                    strncpy(send_packet.data, input_buffer, sizeof(send_packet.data) - 1);
                    size_t msg_len = strlen(send_packet.data);
                    current_seq_num += msg_len;
                    send_packet.header.seq_num = htonl(current_seq_num);
                    sendto(sockFD, &send_packet, sizeof(send_packet.header) + msg_len, 0, (const struct sockaddr *)&client_addr, client_len);
                    printf("> ");
                    fflush(stdout);
                }
            }

            // Check for network packet from client
            if (FD_ISSET(sockFD, &readfds)) {
                memset(&recv_packet, 0, sizeof(recv_packet));
                bytesReceived = recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &client_len);
                if (bytesReceived > 0) {
                    if (ntohs(recv_packet.header.flags) & FIN) {
                        printf("\n[INFO] Client initiated termination.\n");
                        keep_chatting = false;
                    } else {
                        printf("\nClient: %s> ", recv_packet.data);
                        fflush(stdout);
                    }
                }
            }
        }
    } else {
        // --- FILE TRANSFER (RECEIVER) ---
        uint32_t expected_seq_num = 0;
        FILE *outfile = NULL;
        received_packet_info recv_buffer[WINDOW_SIZE] = {0};

        while(1) {
            bytesReceived = recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &client_len);
            if (bytesReceived <= 0) continue;

            uint16_t flags = ntohs(recv_packet.header.flags);
            uint32_t current_seq_num = ntohl(recv_packet.header.seq_num);

            if (flags & FIN) {
                 printf("[TERMINATION] Received FIN from client.\n");
                 break; 
            }
            
            if (flags & FILENAME_FLAG) {
                outfile = fopen(recv_packet.data, "wb");
                if (!outfile) { perror("Failed to create output file"); break; }
                printf("[TRANSFER] Receiving file, will be saved as '%s'\n", recv_packet.data);
                expected_seq_num = ntohl(recv_packet.header.seq_num);
            } else { // It's a regular data packet
                if (!outfile) { printf("[WARN] Received data packet before filename. Ignoring.\n"); continue; }
                
                size_t data_len = bytesReceived - sizeof(sham_header);

                if (current_seq_num == expected_seq_num) {
                    printf("<- Received IN-ORDER packet SEQ=%u\n", current_seq_num);
                    fwrite(recv_packet.data, 1, data_len, outfile);
                    expected_seq_num += data_len;
                    
                    bool check_buffer = true;
                    while(check_buffer) {
                        check_buffer = false;
                        int buffer_idx = (expected_seq_num / PAYLOAD_SIZE) % WINDOW_SIZE;
                        if(recv_buffer[buffer_idx].received) {
                            printf("<- Pulled SEQ=%u from buffer.\n", expected_seq_num);
                            fwrite(recv_buffer[buffer_idx].packet.data, 1, recv_buffer[buffer_idx].len - sizeof(sham_header), outfile);
                            expected_seq_num += (recv_buffer[buffer_idx].len - sizeof(sham_header));
                            recv_buffer[buffer_idx].received = false;
                            check_buffer = true;
                        }
                    }
                } else if (current_seq_num > expected_seq_num) {
                    int buffer_idx = (current_seq_num / PAYLOAD_SIZE) % WINDOW_SIZE;
                    if(!recv_buffer[buffer_idx].received) {
                       recv_buffer[buffer_idx].packet = recv_packet;
                       recv_buffer[buffer_idx].len = bytesReceived;
                       recv_buffer[buffer_idx].received = true;
                       printf("<- Buffered OUT-OF-ORDER packet SEQ=%u (expected %u)\n", current_seq_num, expected_seq_num);
                    }
                } else {
                     printf("<- Received DUPLICATE packet SEQ=%u (expected %u)\n", current_seq_num, expected_seq_num);
                }
            }
            
            if (((double)rand() / RAND_MAX) >= loss_rate) {
                memset(&send_packet, 0, sizeof(send_packet));
                send_packet.header.ack_num = htonl(expected_seq_num);
                send_packet.header.flags = htons(ACK);
                send_packet.header.window_size = htons(MAX_BUFFER_SIZE);
                sendto(sockFD, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&client_addr, client_len);
                printf("-> Sent cumulative ACK=%u\n", expected_seq_num);
            } else {
                printf("-> Dropped cumulative ACK=%u (simulated loss)\n", expected_seq_num);
            }
        }
        if(outfile) {
            fclose(outfile);
            printf("\n[STATUS] File transfer finished for %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
    }

    // --- Termination Handshake ---
    printf("\n[TERMINATION] Starting termination for %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    uint32_t client_fin_seq = ntohl(recv_packet.header.seq_num);
    
    memset(&send_packet, 0, sizeof(send_packet)); // Reuse send_packet
    send_packet.header.ack_num = htonl(client_fin_seq + 1);
    send_packet.header.flags = htons(ACK);
    sendto(sockFD, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&client_addr, client_len);
    printf("[TERMINATION] Step 1: Sent ACK for client's FIN.\n");
    
    send_packet.header.seq_num = htonl(server_seq_num_y + 1);
    send_packet.header.flags = htons(FIN);
    sendto(sockFD, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&client_addr, client_len);
    printf("[TERMINATION] Step 2: Sent our own FIN.\n");
    
    recvfrom(sockFD, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
    if(ntohs(recv_packet.header.flags) & ACK) {
        printf("[TERMINATION] Step 3: Received final ACK.\n");
    } else {
        printf("[WARN] Did not receive final ACK, but closing session anyway.\n");
    }
    printf("[STATUS] Connection closed with %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}


