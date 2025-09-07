#include "sham.h"

int main(int argc, char *argv[]) {
    // --- Argument Parsing ---
    if (argc < 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  File Transfer: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
        fprintf(stderr, "  Chat:          %s <server_ip> <server_port> --chat [loss_rate]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int chat_mode = 0;
    char *input_file = NULL;
    char *output_file_name = NULL;
    double loss_rate = 0.0;

    // Check for chat mode
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--chat") == 0) {
            chat_mode = 1;
            break;
        }
    }

    if (chat_mode) {
        if (argc > 4) {
            loss_rate = atof(argv[4]);
        }
        printf("[CONFIG] Mode: Chat | Loss Rate: %.2f\n", loss_rate);
    } else { // File Transfer Mode
        if (argc < 5) {
            fprintf(stderr, "Error: Missing <input_file> and <output_file_name> for file transfer mode.\n");
            exit(EXIT_FAILURE);
        }
        input_file = argv[3];
        output_file_name = argv[4];
        if (argc > 5) {
            loss_rate = atof(argv[5]);
        }
        printf("[CONFIG] Mode: File Transfer | Input: %s | Output: %s | Loss Rate: %.2f\n", input_file, output_file_name, loss_rate);
    }
    
    // Seed the random number generator
    srand(time(NULL));

    // --- Socket Creation and Setup ---
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    Packet send_packet;
    Packet recv_packet;

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Clear and set server address information
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    // Convert text IP address to binary format
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("invalid server IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[STATUS] Client starting up...\n");

    // --- Three-Way Handshake with Retries ---
    int handshake_attempts = 0;
    const int MAX_HANDSHAKE_ATTEMPTS = 3;
    int handshake_success = 0;
    uint32_t client_seq_num_x = rand() % 10000; // Generate a random initial sequence number

    while (handshake_attempts < MAX_HANDSHAKE_ATTEMPTS && !handshake_success) {
        handshake_attempts++;

        // 1. Send SYN to Server
        memset(&send_packet, 0, sizeof(send_packet));
        send_packet.header.seq_num = htonl(client_seq_num_x);
        send_packet.header.flags = htons(SYN);
        send_packet.header.window_size = htons(5000); // Client's initial buffer size

        printf("\n[HANDSHAKE ATTEMPT %d] Step 1: Sending SYN\n", handshake_attempts);
        packetDebugPrint(&send_packet, "-> SEND");

        if (sendto(sockfd, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&server_addr, server_len) < 0) {
            perror("sendto failed for SYN packet");
            continue; // Try again
        }
        
        // 2. Wait for SYN-ACK with a timeout
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 2; // 2-second timeout
        timeout.tv_usec = 0;

        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select error during handshake");
            break; 
        }

        if (activity == 0) {
            printf("[TIMEOUT] No SYN-ACK received from server.\n");
            continue; // This will trigger the next attempt
        }

        // If we're here, data is ready to be read
        ssize_t len = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
        if (len < 0) {
            perror("recvfrom failed while waiting for SYN-ACK");
            continue; // Try again
        }

        // Validate the SYN-ACK packet
        uint16_t received_flags = ntohs(recv_packet.header.flags);
        uint32_t received_ack_num = ntohl(recv_packet.header.ack_num);

        if ((received_flags == (SYN | ACK)) && (received_ack_num == client_seq_num_x + 1)) {
            printf("[HANDSHAKE] Step 2: Received valid SYN-ACK\n");
            packetDebugPrint(&recv_packet, "<- RECV");
            handshake_success = 1;
        } else {
            fprintf(stderr, "[ERROR] Invalid SYN-ACK received from server.\n");
            // We'll let the loop retry
        }
    }

    if (!handshake_success) {
        fprintf(stderr, "[FATAL] Handshake failed after %d attempts. Exiting.\n", MAX_HANDSHAKE_ATTEMPTS);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Store server's sequence number for the final ACK
    uint32_t server_seq_num_y = ntohl(recv_packet.header.seq_num);

    // 3. Send final ACK to Server
    memset(&send_packet, 0, sizeof(send_packet));
    send_packet.header.seq_num = htonl(client_seq_num_x + 1);
    send_packet.header.ack_num = htonl(server_seq_num_y + 1);
    send_packet.header.flags = htons(ACK);
    send_packet.header.window_size = htons(5000);

    printf("[HANDSHAKE] Step 3: Sending final ACK\n");
    packetDebugPrint(&send_packet, "-> SEND");

    if (sendto(sockfd, &send_packet, sizeof(send_packet.header), 0, (const struct sockaddr *)&server_addr, server_len) < 0) {
        perror("sendto failed for final ACK");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("\n[STATUS] Connection established with %s:%d.\n", server_ip, port);
    printf("---------------------------------------------\n");
    
    // Application logic will now depend on the mode
    if (chat_mode) {
        printf("[INFO] Chat mode active. Ready for user input.\n");
        // TODO: Implement chat logic using select()
    } else {
        printf("[INFO] File transfer mode active. Preparing to send '%s'.\n", input_file);
        // TODO: Implement file transfer logic
    }
    
    close(sockfd);
    return 0;
}


