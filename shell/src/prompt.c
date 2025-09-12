#include "../include/prompt.h"
#include "../include/utilities.h"

void printShellPrompt(char * homeDirectory) {
    char *username = getenv("USER");
    if(!username) username = "user";

    char hostname[256];
    if(gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "host");
    }

    char buffer[1024];
    char *currDirectory = getcwd(buffer, sizeof(buffer));
    if(!currDirectory) {
        printf("getcwd failed");
        exit(1);
    }

    char *commonDirectory = getSubstr(currDirectory, 0, strlen(homeDirectory));
    char *relDirectory = getSubstr(currDirectory, strlen(homeDirectory), strlen(currDirectory) - strlen(homeDirectory));

    if(commonDirectory && !strcmp(commonDirectory, homeDirectory)) {
        if(relDirectory && strlen(relDirectory) > 0) {
            printf("<%s@%s:~%s> ", username, hostname, relDirectory);
        } else {
            printf("<%s@%s:~> ", username, hostname);
        }
    } else {
        printf("<%s@%s:%s> ", username, hostname, currDirectory);
    }

    free(commonDirectory);
    free(relDirectory);
}

