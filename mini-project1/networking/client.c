#include "sham.h"

int main(int argc, char *argv[]) {
    if(argc < 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  File Transfer: %s <ip> <port> <infile> <outfile> [loss_rate]\n", argv[0]);
        fprintf(stderr, "  Chat: %s <ip> <port> --chat [loss_rate]\n", argv[0]);
        exit(EXIT_FAILURE);
    }




