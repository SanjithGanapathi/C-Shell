#include "../include/shell.h"
#include "../include/parser.h"
#include "../include/execute.h"
#include "../include/utilities.h"

#define MAX_COMMAND 1025

char * homeDirectory = NULL;
char * prevDir = NULL;


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
            printf("<%s@%s:~%s>", username, hostname, relDirectory);
        } else {
            printf("<%s@%s:~>", username, hostname);
        }
    } else {
        printf("<%s@%s:%s>", username, hostname, currDirectory);
    }

    free(commonDirectory);
    free(relDirectory);
}

int main() {
    char *command = (char*)malloc(MAX_COMMAND);
    if(!command) {
        printf("malloc failed");
        exit(1);
    }

    char buffer[1024];
    homeDirectory = NULL;
    homeDirectory = getcwd(buffer, sizeof(buffer));
    if(!homeDirectory) {
        printf("getcwd failed");
        exit(1);
    }

    while(true) {
        printShellPrompt(homeDirectory);
        if(fgets(command, MAX_COMMAND, stdin) == NULL) continue;

        command[strcspn(command, "\n")] = '\0';
        if(strcmp(command, "exit") == 0) break;

        ShellCmd * parsedCmd = parseCommand(command);
        if(parsedCmd) {
            printf("Input is valid\n");
        } else {
            printf("Invalid input\n");
        }
     
        if(executeCommand(parsedCmd)) {
            printf("Command executed succesfully\n");
        } else {
            printf("Invalid Command\nExiting....\n");
            exit(1);
        }
    }
    free(command);
    return 0;
}

