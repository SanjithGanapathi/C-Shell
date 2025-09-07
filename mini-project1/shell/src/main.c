#include "../include/shell.h"
#include "../include/parser.h"
#include "../include/execute.h"
#include "../include/utilities.h"
#include "../include/prompt.h"

#define MAX_COMMAND 1025

char * homeDirectory = NULL;
char * prevDir = NULL;
int nextJobID = 1;
Job **backgroundJobs = NULL;

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


    initializeJobControl();
    while(true) {
        printShellPrompt(homeDirectory);
        if(fgets(command, MAX_COMMAND, stdin) == NULL) continue;

        command[strcspn(command, "\n")] = '\0';
        if(strcmp(command, "exit") == 0) break;

        checkBackgroundJobs();
        ShellCmd * parsedCmd = parseCommand(command);
        if(!parsedCmd) {
            printf("Invalid Syntax!\n");
            fflush(stdout);
            continue;
        }
/*
        for(int i = 0; i<parsedCmd->groupCount; i++) {
            printf("%d ", parsedCmd->background);
        }
*/
        if(executeCommand(parsedCmd, command)) {
//            printf("Command executed succesfully\n");
        } else {
//            printf("Invalid Command\nExiting....\n");
            exit(1);
        }
    }
    free(command);
    return 0;
}

