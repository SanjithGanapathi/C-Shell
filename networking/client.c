#include "sham.h"

int main(int argc, char *argv[]) {
    // --- Logging Setup ---
    char *log_env = getenv("RUDP_LOG");
    if (log_env && strcmp(log_env, "1") == 0) {
        log_file = fopen("client_log.txt", "w");
        if (log_file == NULL) {
            perror("Failed to open client_log.txt");
            // Continue without logging
        }
    }

    // --- Argument Parsing (Unchanged) ---
    if (argc < 4) { fprintf(stderr, "Usage...\n"); exit(EXIT_FAILURE); }
    char *server_ip = argv[1]; int port = atoi(argv[2]); int chat_mode = 0;
    char *input_file = NULL; 
    char *output_file_name = NULL; 
    double loss_rate = 0.0;
    for (int i = 3; i < argc; i++) { 
        if (strcmp(argv[i], "--chat") == 0) { 
            chat_mode = 1; break; 
        } 
    }
    if (chat_mode) { 
        if (argc > 4) 
            loss_rate = atof(argv[4]);
        printf("[CONFIG] Mode: Chat | Loss Rate: %.2f\n", loss_rate);
    } else { if (argc < 5) { 
        fprintf(stderr, "Error: Missing file names.\n"); 
        exit(EXIT_FAILURE); }
        input_file = argv[3]; 
        output_file_name = argv[4]; 
        if (argc > 5) 
            loss_rate = atof(argv[5]);
        printf("[CONFIG] Mode: File Transfer | Input: %s | Output: %s | Loss Rate: %.2f\n", input_file, output_file_name, loss_rate);
    }

    // --- Socket Creation (Unchanged) ---
    srand(time(NULL)); 
    int sockfd; 
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr); 
    Packet send_packet, recv_packet;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        perror("socket creation failed"); exit(EXIT_FAILURE); 
    }
    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) { 
        perror("invalid server IP"); 
        close(sockfd); 
        exit(EXIT_FAILURE); 
    }
    printf("[STATUS] Client starting up...\n");

    // --- Three-Way Handshake ---
    bool handshake_success = false; 
    uint32_t client_seq_num_x = rand() % 10000;
    // (Handshake retry loop is unchanged, but with logging calls added)
    for (int attempts = 0; attempts < 3 && !handshake_success; attempts++) {
        memset(&send_packet, 0, sizeof(send_packet));
        send_packet.header.seq_num = htonl(client_seq_num_x);
        send_packet.header.flags = htons(SYN);
        send_packet.header.window_size = htons(MAX_BUFFER_SIZE);
        printf("\n[HANDSHAKE ATTEMPT %d] Step 1: Sending SYN\n", attempts + 1);
        packetDebugPrint(&send_packet, "-> SEND");
        log_event("SND SYN SEQ=%u", client_seq_num_x);
        sendto(sockfd, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&server_addr, server_len);

        fd_set readfds; 
        FD_ZERO(&readfds); 
        FD_SET(sockfd, &readfds);
        struct timeval timeout = { 
            .tv_sec = 2, 
            .tv_usec = 0 
        };
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity > 0) {
            recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
            if ((ntohs(recv_packet.header.flags) == (SYN | ACK)) && (ntohl(recv_packet.header.ack_num) == client_seq_num_x + 1)) {
                printf("[HANDSHAKE] Step 2: Received valid SYN-ACK\n");
                packetDebugPrint(&recv_packet, "<- RECV");
                log_event("RCV SYN-ACK SEQ=%u ACK=%u", ntohl(recv_packet.header.seq_num), ntohl(recv_packet.header.ack_num));
                handshake_success = true;
            } else { 
                fprintf(stderr, "[ERROR] Invalid SYN-ACK received.\n"); 
            }
        } else { 
            printf("[TIMEOUT] No SYN-ACK received.\n"); 
        }
    }
    if (!handshake_success) { 
        fprintf(stderr, "[FATAL] Handshake failed. Exiting.\n"); 
        close(sockfd); 
        exit(EXIT_FAILURE); 
    }

    uint32_t server_seq_num_y = ntohl(recv_packet.header.seq_num);
    memset(&send_packet, 0, sizeof(send_packet));
    send_packet.header.seq_num = htonl(client_seq_num_x + 1);
    send_packet.header.ack_num = htonl(server_seq_num_y + 1);
    send_packet.header.flags = htons(ACK);
    printf("[HANDSHAKE] Step 3: Sending final ACK\n");
    packetDebugPrint(&send_packet, "-> SEND");
    log_event("SND ACK FOR SYN");
    sendto(sockfd, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&server_addr, server_len);
    printf("\n[STATUS] Connection established.\n---------------------------------------------\n");
    
    // --- Application Logic ---
    uint32_t base_seq = client_seq_num_x + 1; 
    uint32_t next_seq = client_seq_num_x + 1;
    uint32_t receiver_window = ntohs(recv_packet.header.window_size);
    log_event("FLOW WIN UPDATE=%u", receiver_window);

    if (chat_mode) {
        printf("[INFO] Chat mode active. Type '/quit' to exit.\n> ");
        fflush(stdout);

        bool keep_chatting = true;
        uint32_t current_seq_num = client_seq_num_x + 1;

        // ***** FIX: ADDED MISSING while LOOP FOR CHAT *****
        while (keep_chatting) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            FD_SET(sockfd, &readfds);

            int max_fd = (STDIN_FILENO > sockfd) ? STDIN_FILENO : sockfd;
            int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

            if ((activity < 0) && (errno != EINTR)) {
                perror("select error");
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                char input_buffer[PAYLOAD_SIZE];
                if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
                    if (strncmp(input_buffer, "/quit", 5) == 0) {
                        printf("[INFO] Quit command received. Initiating termination...\n");
                        keep_chatting = false;
                        continue;
                    }

                    memset(&send_packet, 0, sizeof(send_packet));
                    strncpy(send_packet.data, input_buffer, sizeof(send_packet.data) - 1);
                    size_t msg_len = strlen(send_packet.data);
                    current_seq_num += msg_len;
                    send_packet.header.seq_num = htonl(current_seq_num);

                    if (((double)rand() / RAND_MAX) >= loss_rate) {
                        // If the random number is high enough, send the packet
                        sendto(sockfd, &send_packet, sizeof(send_packet.header) + msg_len, 0, (const struct sockaddr *)&server_addr, server_len);
                    } else {
                        // Otherwise, "drop" the packet by not sending it and print a message
                        printf("-> Dropped CHAT message (simulated loss)\n");
                        log_event("DROP DATA SEQ=%u", current_seq_num);
                    }
                    printf("> ");
                    fflush(stdout);
                }
            }

            if (FD_ISSET(sockfd, &readfds)) {
                memset(&recv_packet, 0, sizeof(recv_packet));
                ssize_t len = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
                if (len > 0) {
                     if (ntohs(recv_packet.header.flags) & FIN) {
                        printf("\n[INFO] Server initiated termination.\n");
                        keep_chatting = false;
                    } else {
                        printf("\nServer: %s> ", recv_packet.data);
                        fflush(stdout);
                    }
                }
            }
        }
    } else {
        // --- FILE TRANSFER ---
        FILE *file = fopen(input_file, "rb");
        if (!file) { perror("Failed to open input file"); close(sockfd); return 1; }
        
        printf("[TRANSFER] Sending output filename: %s\n", output_file_name);
        memset(&send_packet, 0, sizeof(send_packet));
        next_seq += 1; // Use a sequence number for this control packet
        send_packet.header.seq_num = htonl(next_seq);
        send_packet.header.flags = htons(FILENAME_FLAG | ACK);
        strncpy(send_packet.data, output_file_name, PAYLOAD_SIZE - 1);
        sendto(sockfd, &send_packet, sizeof(send_packet.header) + strlen(output_file_name) + 1, 0, (const struct sockaddr*)&server_addr, server_len);

        // Wait for server to ACK the filename
        recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
        base_seq = ntohl(recv_packet.header.ack_num);
        next_seq = base_seq;
        printf("[TRANSFER] Filename ACK'd. Starting data transmission.\n");        // (Sending filename is unchanged)

        sent_packet_info window_buffer[WINDOW_SIZE] = {0}; bool file_done = false;
        while (!file_done || base_seq < next_seq) {
            while (((next_seq - base_seq) / PAYLOAD_SIZE < WINDOW_SIZE) && ((next_seq - base_seq) < receiver_window) && !file_done) {
                char file_buf[PAYLOAD_SIZE];
                size_t bytes_read = fread(file_buf, 1, PAYLOAD_SIZE, file);
                if (bytes_read > 0) {
                    Packet data_p; memset(&data_p, 0, sizeof(data_p));
                    data_p.header.seq_num = htonl(next_seq);
                    data_p.header.flags = htons(ACK);
                    memcpy(data_p.data, file_buf, bytes_read);
                    size_t packet_len = sizeof(data_p.header) + bytes_read;

                    if (((double)rand() / RAND_MAX) >= loss_rate) {
                        sendto(sockfd, &data_p, packet_len, 0, (const struct sockaddr*)&server_addr, server_len);
                        log_event("SND DATA SEQ=%u LEN=%zu", next_seq, bytes_read);
                    } else {
                        printf("-> Dropped DATA packet with SEQ=%u (simulated loss)\n", next_seq);
                        log_event("DROP DATA SEQ=%u", next_seq);
                    }
                    int window_idx = (next_seq / PAYLOAD_SIZE) % WINDOW_SIZE;
                    window_buffer[window_idx].packet = data_p;
                    window_buffer[window_idx].packet_len = packet_len;
                    gettimeofday(&window_buffer[window_idx].time_sent, NULL);
                    next_seq += bytes_read;
                } else { 
                    if (feof(file)) { 
                        file_done = true; 
                    } else { 
                        perror("File read error"); break; 
                    } 
                }
            }

            fd_set readfds; FD_ZERO(&readfds); FD_SET(sockfd, &readfds);
            struct timeval timeout = { 
                .tv_sec = 0, 
                .tv_usec = RTO_MS * 1000 
            };
            int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

            if (activity > 0) {
                recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
                if (ntohs(recv_packet.header.flags) & ACK) {
                    uint32_t acked_seq = ntohl(recv_packet.header.ack_num);
                    log_event("RCV ACK=%u", acked_seq);
                    if (acked_seq > base_seq) { base_seq = acked_seq; }
                    uint16_t new_window = ntohs(recv_packet.header.window_size);
                    if (new_window != receiver_window) {
                        receiver_window = new_window;
                        log_event("FLOW WIN UPDATE=%u", receiver_window);
                    }
                }
            } else if (activity == 0) {
                if (base_seq < next_seq) {
                    log_event("TIMEOUT SEQ=%u", base_seq);
                    int window_idx = (base_seq / PAYLOAD_SIZE) % WINDOW_SIZE;
                    sendto(sockfd, &window_buffer[window_idx].packet, window_buffer[window_idx].packet_len, 0, (const struct sockaddr*)&server_addr, server_len);
                    size_t retransmitted_len = window_buffer[window_idx].packet_len - sizeof(sham_header);
                    log_event("RETX DATA SEQ=%u LEN=%zu", base_seq, retransmitted_len);
                    gettimeofday(&window_buffer[window_idx].time_sent, NULL);
                }
            }
        }
        fclose(file); printf("\n[STATUS] File transfer complete.\n");
    }

    // --- Four-Way Handshake ---
    printf("\n---------------------------------------------\n[TERMINATION] Starting Four-Way Handshake.\n");
    uint32_t client_fin_seq = next_seq;
    memset(&send_packet, 0, sizeof(send_packet));
    send_packet.header.seq_num = htonl(client_fin_seq);
    send_packet.header.flags = htons(FIN);
    log_event("SND FIN SEQ=%u", client_fin_seq);
    sendto(sockfd, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&server_addr, server_len);
    printf("[TERMINATION] Step 1: Sent FIN.\n");

    recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
    log_event("RCV ACK FOR FIN");
    printf("[TERMINATION] Step 2: Received ACK for our FIN.\n");

    recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
    uint32_t server_fin_seq = ntohl(recv_packet.header.seq_num);
    log_event("RCV FIN SEQ=%u", server_fin_seq);
    printf("[TERMINATION] Step 3: Received FIN from server.\n");

    send_packet.header.seq_num = htonl(client_fin_seq + 1);
    send_packet.header.ack_num = htonl(server_fin_seq + 1);
    send_packet.header.flags = htons(ACK);
    log_event("SND ACK=%u", server_fin_seq + 1);
    sendto(sockfd, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&server_addr, server_len);
    printf("[TERMINATION] Step 4: Sent final ACK. Connection closing.\n");

    // --- Cleanup ---
    if (log_file) { 
        fclose(log_file); 
    }
    close(sockfd);
    return 0;
}


