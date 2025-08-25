#include "../include/shell.h"
#include "../include/execute.h"
#include "../include/utilities.h"

bool executeAtomicCmd(AtomicCmd * cmd) {
    if(!strcmp("hop", cmd->argv[0])) {
        executeHop(cmd);
    } else if(!strcmp("reveal", cmd->argv[0])) {
        executeReveal(cmd); 
    } else if(!strcmp("log", cmd->argv[0])) {
//        executeLog(cmd);   
    } else {
        executeArbitaryCommands(cmd);
    }
    return true;
}

bool executeArbitaryCommands(AtomicCmd * cmd) {
    int f = fork();
    if(f < 0) {
        printf("Fork Failure\n");
        exit(1);
    } else if(f == 0) {
        if(cmd->inputFile != NULL) {
            int newFD = open(cmd->inputFile, O_RDONLY);
            if(newFD == -1) {
                printf("Error in open");
                return false;
            }
            dup2(newFD, 0);
            close(newFD);
        }

        if(cmd->outputFile != NULL) {
            int newFD;
            if(cmd->append) newFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else newFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(newFD == -1) {
                printf("Error in open");
                return false;
            }
            dup2(newFD, 1);
            close(newFD);
        }
        execvp(cmd->argv[0], cmd->argv);
        printf("Execution Failure\n");
        return false;
    } else {
        int status;
        waitpid(f, &status, 0);
    }
    return true; 
}

bool executeCmdGroup(CmdGroup * cmd) {
    int i = 0;
    while(i < cmd->cmdCount) {
        if(executeAtomicCmd(cmd->commands[i]) == false) {
            printf("Error executing the cmdgroup\n");
            return false;
        }
        i++;
    }
    return true;
}

void printCmd(CmdGroup * cmd) {
    // print the command as it is
}

bool executeCommand(ShellCmd * cmd) {
    int i = 0;
    while(i < cmd->groupCount) {
        // Background execution stuff;
        if(cmd->background) {
        } else {
            if(executeCmdGroup(cmd->groups[i]) == false) {
                printf("Error executing the cmdgroup\n");
                return false;
            }
        }
        i++;
    }
    return true;
}

bool executeHop(AtomicCmd * cmd) {
    // case when there is no arguments so change to homeDirectory and exit
    if(cmd->argv[1] == NULL) {
        char buffer[MID_PATH_MAX]; // use MID_PATH_MAX from defined constants
        if(getcwd(buffer, sizeof(buffer)) == NULL) {
            printf("Failure to fetch Dir\n");
            return false;
        }
        if(chdir(homeDirectory) == -1) {
            printf("Error in moving back to homeDirectory\n");
            return false;
        }
        // freeing the old path before assigning the new one
        free(prevDir);
        prevDir = strdup(buffer);
        return true;
    }

    int i = 1;
    while(cmd->argv[i] != NULL) {
        char * symbol = cmd->argv[i];
        char buffer[MID_PATH_MAX];

        // FIX: Get the current directory *before* any changes are made.
        if(getcwd(buffer, sizeof(buffer)) == NULL) {
            printf("Failure to fetch Dir\n");
            return false;
        }

        if(!strcmp(symbol, "-")) {
            if(prevDir == NULL) {
                printf("OLDPWD not set\n");
            } else {
                if(chdir(prevDir) == -1) {
                    printf("No such directory\n");
                    return false;
                }
                // now set the old CWD (now in buffer) so it becomes the new prevDir.
                free(prevDir);
                prevDir = strdup(buffer);
            }
        } else if(!strcmp(symbol, ".")) {
            // here do nothing.
        } else if(!strcmp(symbol, "..")) {
            if(chdir("..") == -1) {
                printf("No such directory!\n");
                return false;
            }
            // update prevDir with a permanent copy of the old path.
            free(prevDir);
            prevDir = strdup(buffer);
        } else if(!strcmp(symbol, "~")) {
            if(chdir(homeDirectory) == -1) {
                printf("No such directory!\n");
                return false;
            }
            // update prevDir with a permanent copy of the old path.
            free(prevDir);
            prevDir = strdup(buffer);
        } else {
            // this block handles paths like "dir", "../dir", and "~/dir"
            if(symbol[0] == '~') {
                char full_path[MID_PATH_MAX];
                sprintf(full_path, "%s%s", homeDirectory, symbol + 1);
                if(chdir(full_path) == -1) {
                    printf("No such directory!\n");
                    return false;
                }
            } else {
                if(chdir(symbol) == -1) {
                    printf("No such directory!\n");
                    return false;
                }
            }
            free(prevDir);
            prevDir = strdup(buffer);
        }
        i++;
    }
    return true;
}

bool executeReveal(AtomicCmd * cmd) {
    bool flagA = false;
    bool flagL = false; 
    char * targetPath = NULL;

    int i = 1;
    while(cmd->argv[i] != NULL) {
        char * symbol = cmd->argv[i];
        if(symbol[0] == '-' && strlen(symbol) != (size_t)(1)) {
            // check for 'a' and 'l' flags
            if(strchr(symbol, 'a')) flagA = true;
            if(strchr(symbol, 'l')) flagL = true;
        } else {
            targetPath = symbol;
        }
        i++;
    }

    if(targetPath == NULL) {
        targetPath = "."; // just setting it to '.' as default case
    } else if(!strcmp(targetPath, "~")) {
        targetPath = homeDirectory;
    } else if(!strcmp(targetPath, "-")) {
        if(prevDir == NULL) {
            printf("OLDPWD not set\n");
            return false;
        }
        targetPath = prevDir;
    }

    DIR * targetDir = opendir(targetPath);
    if(targetDir == NULL) {
        printf("No such directory!\n");
        return false;
    }

    struct dirent *dirEntry;
    int maxFileCnt = 20;
    int cnt = 0;
    char ** files = (char **)malloc(sizeof(char*)*maxFileCnt);
    if(files == NULL) {
        closedir(targetDir);
        printf("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    while((dirEntry = readdir(targetDir)) != NULL) {
        // if -a flag is set then show hidden files
        if(!flagA && dirEntry->d_name[0] == '.') {
            continue;
        }

        if(cnt >= maxFileCnt) {
            maxFileCnt *= 2;
            files = realloc(files, sizeof(char*)*maxFileCnt);
        }
        files[cnt++] = dirEntry->d_name;
    }

    qsort(files, cnt, sizeof(char *), compareStrings);
    
    if(flagL) {
        for(int i = 0; i<cnt; i++) printf("%s\n", files[i]);
    } else {
        for(int i = 0; i<cnt; i++) {
            printf("%s ", files[i]);
            if(i == cnt-1) printf("\n");
        }
    }

    closedir(targetDir);
    return true;
}
