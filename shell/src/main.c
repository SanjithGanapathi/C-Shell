#include "../include/shell.h"
#include "../include/parser.h"
#include "../include/execute.h"
#include "../include/utilities.h"
#include "../include/prompt.h"

#define MAX_COMMAND 1025

// Global Variables 
char * homeDirectory = NULL;
char * prevDir = NULL;
bool is_interactive = false; // This will be set once at the start
int nextJobID = 1;
char * cmdHistoryFile = NULL;

int main() {
    // HIGHLIGHT: Determine if the shell is running in an interactive terminal.
    is_interactive = isatty(STDIN_FILENO);
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

    // HIGHLIGHT: Signal handling and job control setup MUST happen
    // regardless of whether the shell is interactive or not. This is the main fix.
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // These initializations are correct.
    initializeHistory();
    initializeJobControl();

    while(true) {
        // HIGHLIGHT: Printing the prompt is a UI feature, so it correctly
        // stays inside the is_interactive check.
        if(is_interactive) {
            printShellPrompt(homeDirectory);
        }
            
        if(fgets(command, MAX_COMMAND, stdin) == NULL) {
            /* Ctrl-D (EOF) handling: kill any remaining background jobs/process groups */
            for(int i = 0; i < jobCnt; i++) {
                if(backgroundJobs[i]) {
                    /* Send SIGKILL to the entire process group */
                    kill(-backgroundJobs[i]->pid, SIGKILL);
                }
            }
            /* Best-effort reap (non-blocking) */
            for(int i = 0; i < jobCnt; i++) {
                int status;
                waitpid(backgroundJobs[i]->pid, &status, WNOHANG);
            }
            if(is_interactive) {
                printf("logout\n");
            }
            break; // Exit the loop
        }

        checkBackgroundJobs();

        command[strcspn(command, "\n")] = '\0';
        if(strlen(command) == 0) continue;
        if(strcmp(command, "exit") == 0) break;

        ShellCmd * parsedCmd = parseCommand(command);
        
        if(parsedCmd) {
            executeCommand(parsedCmd, command);
            addCmd(command);
            freeShellCmd(parsedCmd);    
        }
    }

    // Cleanup  
    saveToLogFile(); // Save to cmdHistoryFile 
    free(command); // free the command
    cleanupJobControl(); // free the jobs in backgroundJobs 
    return 0;
}
