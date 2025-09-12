#include "../include/shell.h"
#include "../include/execute.h"
#include "../include/utilities.h"

// Static variables for Job handling
Job **backgroundJobs = NULL;
int jobCnt = 0;
int jobCapacity = 10;

// Static variables for Log History
static char *cmdLog[MAX_HISTORY];
static int logCnt = 0;
static int nextLog = 0;
volatile pid_t foreground_pgid = 0;
/* HIGHLIGHT: track original shell pid to distinguish parent vs pipeline child for log execute passthrough */
static pid_t shell_pid = 0;

// JOB CONTROL INITIALIZATION
void initializeJobControl() {
    jobCapacity = 10; // Initial capacity for 10 jobs
    backgroundJobs = malloc(sizeof(Job *) * jobCapacity);
    if(backgroundJobs == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

void sigint_handler(int signum) {
    // Check if there is a foreground process running.
    if(foreground_pgid > 0) {
        // Send the SIGINT signal to the entire foreground process group.
        kill(-foreground_pgid, SIGINT);
    }
}

// This function will be called when the user presses Ctrl-Z.
void sigtstp_handler(int signum) {
    // Check if there is a foreground process running.
    if(foreground_pgid > 0) {
        // Send the SIGTSTP signal to stop the entire foreground process group.
        kill(-foreground_pgid, SIGTSTP);
    }
}

// JOBS Related functions
Job *initialiseJob(pid_t pid, int jobID, char *command) {
    Job *newJob = (Job *)malloc(sizeof(Job));
    if(!newJob) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    newJob->pid = pid;
    newJob->jobID = jobID;
    newJob->cmdName = strdup(command ? command : "");
    newJob->status = RUNNING; // HIGHLIGHT: initialize status
    return newJob;
}

/* HIGHLIGHT: job lookup helpers */
Job *findJobByID(int jobID) {
    for(int i = 0; i < jobCnt; i++) {
        if(backgroundJobs[i]->jobID == jobID)
            return backgroundJobs[i];
    }
    return NULL;
}

Job *mostRecentJob() {
    if(jobCnt == 0)
        return NULL;
    return backgroundJobs[jobCnt - 1];
}

void cleanupJobControl() {
    for(int i = 0; i < jobCnt; i++) {
        free(backgroundJobs[i]->cmdName);
        free(backgroundJobs[i]);
    }
    free(backgroundJobs);
}

void removeJob(int index) {
    // free the specified index and the string for cmdname
    free(backgroundJobs[index]->cmdName);
    free(backgroundJobs[index]);

    for(int i = index; i < jobCnt - 1; i++) {
        backgroundJobs[i] = backgroundJobs[i + 1];
    }
    jobCnt--;
}

// this function is to check the status of background tasks without stopping them
void checkBackgroundJobs() {
    for(int i = jobCnt - 1; i >= 0; i--) {
        int status;
        pid_t exitPID = waitpid(backgroundJobs[i]->pid, &status, WNOHANG | WUNTRACED);
        if(exitPID > 0) {
            if(WIFSTOPPED(status)) {
                if(backgroundJobs[i]->status != STOPPED) {
                    backgroundJobs[i]->status = STOPPED;
                    fprintf(stdout, "[%d] Stopped %s\n", backgroundJobs[i]->jobID, backgroundJobs[i]->cmdName);
                    fflush(stdout);
                } /* else already reported STOPPED */
            } else if(WIFEXITED(status) || WIFSIGNALED(status)) {
                if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    fprintf(stderr, "%s & with pid %d exited normally\n", backgroundJobs[i]->cmdName, backgroundJobs[i]->pid);
                } else {
                    fprintf(stderr, "%s & with pid %d exited abnormally\n", backgroundJobs[i]->cmdName, backgroundJobs[i]->pid);
                }
                removeJob(i);
            }
        }
    }
}

// EXECUTION Related Commands
void runCommand(AtomicCmd *cmd) {
    /* NEW: Honor full redirection list left-to-right, validating every intermediate target.
       Failure of ANY earlier redirection should abort even if the final one would succeed. */
    if(cmd->redirCount > 0) {
        int final_in = -1;
        int final_out = -1;
        for(int r = 0; r < cmd->redirCount; r++) {
            Redirection *rd = &cmd->redirs[r];
            if(rd->kind == REDIR_IN) {
                int fd = open(rd->path, O_RDONLY);
                if(fd < 0) {
                    fprintf(stderr, "No such file or directory!\n");
                    exit(1);
                }
                if(final_in != -1)
                    close(final_in);
                final_in = fd;
            } else {
                int flags = O_WRONLY | O_CREAT;
                if(rd->kind == REDIR_OUT_APPEND)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;
                int fd = open(rd->path, flags, 0644);
                if(fd < 0) {
                    fprintf(stderr, "Unable to create file for writing\n");
                    exit(1);
                }
                if(final_out != -1)
                    close(final_out);
                final_out = fd;
            }
        }
        if(final_in != -1) {
            dup2(final_in, STDIN_FILENO);
            close(final_in);
        }
        if(final_out != -1) {
            dup2(final_out, STDOUT_FILENO);
            close(final_out);
        }
    } else {
        /* Legacy single-field fallback (kept for safety). */
        if(cmd->inputFile != NULL) {
            int newFD = open(cmd->inputFile, O_RDONLY);
            if(newFD == -1) {
                fprintf(stderr, "No such file or directory!\n");
                exit(1);
            }
            dup2(newFD, STDIN_FILENO);
            close(newFD);
        }
        if(cmd->outputFile != NULL) {
            int newFD;
            if(cmd->append)
                newFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                newFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(newFD == -1) {
                fprintf(stderr, "Unable to create file for writing\n");
                exit(1);
            }
            dup2(newFD, STDOUT_FILENO);
            close(newFD);
        }
    }

    /* Allow certain builtins inside pipelines (run in child). */
    if(!strcmp(cmd->argv[0], "reveal")) {
        executeReveal(cmd);
        exit(0);
    } else if(!strcmp(cmd->argv[0], "log")) {
        executeLog(cmd);
        exit(0);
    } else if(!strcmp(cmd->argv[0], "activities")) {
        executeActivities(cmd);
        exit(0);
    } else if(!strcmp(cmd->argv[0], "ping")) {
        executePing(cmd);
        exit(0);
    }

    execvp(cmd->argv[0], cmd->argv);
    printf("Command not found!\n");
    exit(1);
}

bool executeBuiltin(AtomicCmd *cmd) {
    int stdinFD = dup(STDIN_FILENO);
    int stdoutFD = dup(STDOUT_FILENO);

    /* Unified redirection handling using redirs list if present */
    if(cmd->redirCount > 0) {
        int final_in = -1;
        int final_out = -1;
        for(int r = 0; r < cmd->redirCount; r++) {
            Redirection *rd = &cmd->redirs[r];
            if(rd->kind == REDIR_IN) {
                int fd = open(rd->path, O_RDONLY);
                if(fd < 0) {
                    fprintf(stderr, "No such file or directory!\n");
                    goto restore_fail;
                }
                if(final_in != -1)
                    close(final_in);
                final_in = fd;
            } else {
                int flags = O_WRONLY | O_CREAT;
                if(rd->kind == REDIR_OUT_APPEND)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;
                int fd = open(rd->path, flags, 0644);
                if(fd < 0) {
                    fprintf(stderr, "Unable to create file for writing\n");
                    goto restore_fail;
                }
                if(final_out != -1)
                    close(final_out);
                final_out = fd;
            }
        }
        if(final_in != -1) {
            dup2(final_in, STDIN_FILENO);
            close(final_in);
        }
        if(final_out != -1) {
            dup2(final_out, STDOUT_FILENO);
            close(final_out);
        }
    } else {
        /* Legacy single-field fallback */
        if(cmd->inputFile != NULL) {
            int inputFD = open(cmd->inputFile, O_RDONLY);
            if(inputFD == -1) {
                perror(cmd->inputFile);
                goto restore_fail;
            }
            dup2(inputFD, STDIN_FILENO);
            close(inputFD);
        }
        if(cmd->outputFile != NULL) {
            int outputFD;
            if(cmd->append) {
                outputFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                outputFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if(outputFD == -1) {
                perror(cmd->outputFile);
                goto restore_fail;
            }
            dup2(outputFD, STDOUT_FILENO);
            close(outputFD);
        }
    }

    bool executed = false;
    if(!strcmp("reveal", cmd->argv[0])) {
        executed = executeReveal(cmd);
    } else if(!strcmp("activities", cmd->argv[0])) {
        executed = executeActivities(cmd);
    } else if(!strcmp("log", cmd->argv[0])) {
        executed = executeLog(cmd);
    } else if(!strcmp("ping", cmd->argv[0])) {
        executed = executePing(cmd);
    }

    fflush(stdout);
    dup2(stdinFD, STDIN_FILENO);
    dup2(stdoutFD, STDOUT_FILENO);
    close(stdinFD);
    close(stdoutFD);
    return executed;

restore_fail:
    dup2(stdinFD, STDIN_FILENO);
    dup2(stdoutFD, STDOUT_FILENO);
    close(stdinFD);
    close(stdoutFD);
    return false;
}

bool executeAtomicCmd(AtomicCmd *cmd) {
    /* HIGHLIGHT: Add job-control aware builtins */
    if(!strcmp("fg", cmd->argv[0])) {
        return executeFg(cmd);
    } else if(!strcmp("bg", cmd->argv[0])) {
        return executeBg(cmd);
    } else if(!strcmp("hop", cmd->argv[0])) {
        return executeHop(cmd);
    } else if(!strcmp("reveal", cmd->argv[0])) {
        return executeBuiltin(cmd);
    } else if(!strcmp("log", cmd->argv[0])) {
        return executeBuiltin(cmd);
    } else if(!strcmp("activities", cmd->argv[0])) {
        return executeBuiltin(cmd);
    } else if(!strcmp("ping", cmd->argv[0])) {
        return executeBuiltin(cmd);
    } else {
        pid_t f = fork();
        if(f < 0) {
            perror("fork");
            exit(1);
        } else if(f == 0) {
            setpgid(0, 0);
            /* Restore default handlers in child so it stops/interrupts correctly */
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            runCommand(cmd);
        } else {
            int status;
            setpgid(f, f);
            tcsetpgrp(STDIN_FILENO, f);
            foreground_pgid = f;
            while(1) {
                pid_t w = waitpid(f, &status, WUNTRACED);
                if(w == -1) {
                    if(errno == EINTR)
                        continue;
                    perror("waitpid");
                }
                break;
            }
            if(WIFSTOPPED(status)) {
                if(jobCnt >= jobCapacity) {
                    jobCapacity *= 2;
                    backgroundJobs = realloc(backgroundJobs, sizeof(Job *) * jobCapacity);
                    if(!backgroundJobs) {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                }
                size_t totalLen = 0;
                for(int ai = 0; cmd->argv[ai]; ai++)
                    totalLen += strlen(cmd->argv[ai]) + 1;
                char *fullName = malloc(totalLen + 1);
                if(!fullName) {
                    perror("malloc");
                    exit(EXIT_FAILURE);
                }
                fullName[0] = '\0';
                for(int ai = 0; cmd->argv[ai]; ai++) {
                    strcat(fullName, cmd->argv[ai]);
                    if(cmd->argv[ai + 1])
                        strcat(fullName, " ");
                }
                backgroundJobs[jobCnt] = initialiseJob(f, nextJobID, fullName);
                backgroundJobs[jobCnt]->status = STOPPED;
                printf("[%d] Stopped %s\n", nextJobID, backgroundJobs[jobCnt]->cmdName);
                fflush(stdout);
                free(fullName);
                jobCnt++;
                nextJobID++;
            }
            foreground_pgid = 0;
            tcsetpgrp(STDIN_FILENO, getpgrp());
            return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
    }
    return true;
}

bool executeCmdGroup(CmdGroup *cmd) {
    if(cmd->cmdCount == 1) {
        return executeAtomicCmd(cmd->commands[0]);
    }

    int inputFD = STDIN_FILENO;
    pid_t pidArray[cmd->cmdCount];
    int pipeEnds[2];
    pid_t pgid = -1;

    for(int i = 0; i < cmd->cmdCount; i++) {
        if(i < cmd->cmdCount - 1) {
            if(pipe(pipeEnds) == -1) {
                perror("pipe");
                exit(1);
            }
        }
        pidArray[i] = fork();
        if(pidArray[i] < 0) {
            perror("fork");
            exit(1);
        } else if(pidArray[i] == 0) {
            if(i == 0) {
                setpgid(0, 0);
            } else {
                setpgid(0, pgid);
            }
            /* Restore default signal handling in pipeline children so they
               actually receive and act on SIGINT/SIGTSTP (stop/terminate). */
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if(inputFD != STDIN_FILENO) {
                dup2(inputFD, STDIN_FILENO);
                close(inputFD);
            }
            if(i < cmd->cmdCount - 1) {
                dup2(pipeEnds[1], STDOUT_FILENO);
                close(pipeEnds[0]);
                close(pipeEnds[1]);
            }
            runCommand(cmd->commands[i]);
        } else {
            if(pgid == -1) {
                setpgid(pidArray[i], pidArray[i]);
                pgid = pidArray[i];
            } else {
                setpgid(pidArray[i], pgid);
            }
            if(inputFD != STDIN_FILENO) {
                close(inputFD);
            }
            if(i < cmd->cmdCount - 1) {
                close(pipeEnds[1]);
                inputFD = pipeEnds[0];
            }
        }
    }

    foreground_pgid = pgid;
    tcsetpgrp(STDIN_FILENO, pgid);
    int status;
    int stopped = 0;
    int completed = 0;

    /* Wait on the whole process group so any member receiving SIGTSTP is detected.
       We count normal exits; break immediately on first stop. */
    while(!stopped && completed < cmd->cmdCount) {
        pid_t w = waitpid(-pgid, &status, WUNTRACED);
        if(w < 0) {
            if(errno == EINTR)
                continue;
            perror("waitpid");
            break;
        }
        if(WIFSTOPPED(status)) {
            stopped = 1;
        } else if(WIFEXITED(status) || WIFSIGNALED(status)) {
            completed++;
        }
    }

    if(stopped) {
        if(jobCnt >= jobCapacity) {
            jobCapacity *= 2;
            backgroundJobs = realloc(backgroundJobs, sizeof(Job *) * jobCapacity);
            if(!backgroundJobs) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }
        /* Build full pipeline string: each command's argv joined by spaces, commands separated by ' | ' */
        size_t totalLen = 0;
        for(int ci = 0; ci < cmd->cmdCount; ci++) {
            AtomicCmd *ac = cmd->commands[ci];
            for(int ai = 0; ac->argv[ai]; ai++)
                totalLen += strlen(ac->argv[ai]) + 1;
            if(ci < cmd->cmdCount - 1)
                totalLen += 3;
        }
        char *pipeName = malloc(totalLen + 1);
        if(!pipeName) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        pipeName[0] = '\0';
        for(int ci = 0; ci < cmd->cmdCount; ci++) {
            AtomicCmd *ac = cmd->commands[ci];
            for(int ai = 0; ac->argv[ai]; ai++) {
                strcat(pipeName, ac->argv[ai]);
                if(ac->argv[ai + 1])
                    strcat(pipeName, " ");
            }
            if(ci < cmd->cmdCount - 1)
                strcat(pipeName, " | ");
        }
        backgroundJobs[jobCnt] = initialiseJob(pgid, nextJobID, pipeName);
        backgroundJobs[jobCnt]->status = STOPPED;
        printf("[%d] Stopped %s\n", nextJobID, backgroundJobs[jobCnt]->cmdName);
        fflush(stdout);
        free(pipeName);
        jobCnt++;
        nextJobID++;
    } else {
        /* Reap any remaining children (should be none) without blocking. */
        int s2;
        while(waitpid(-pgid, &s2, WNOHANG) > 0) {
        }
    }

    foreground_pgid = 0;
    tcsetpgrp(STDIN_FILENO, getpgrp());
    return true;
}

void printCmd(CmdGroup *cmd) {
    // print the command as it is
}

bool executeCommand(ShellCmd *cmd, char *command) {
    int i = 0;
    while(i < cmd->groupCount) {
        CmdGroup *group = cmd->groups[i];
        if(group->background) {
            pid_t f = fork();
            if(f < 0) {
                printf("Fork Failure\n");
                return false;
            } else if(f == 0) {
                setpgid(0, 0);
                int devNull = open("/dev/null", O_RDONLY);
                dup2(devNull, STDIN_FILENO);
                close(devNull);
                executeCmdGroup(group);
                exit(0);
            } else {
                setpgid(f, f);
                if(jobCnt >= jobCapacity) {
                    jobCapacity *= 2;
                    backgroundJobs = (Job **)realloc(backgroundJobs, sizeof(Job *) * jobCapacity);
                    if(backgroundJobs == NULL) {
                        printf("Realloc Failure\n");
                        exit(1);
                    }
                }
                /* Build a full command string from the first group's first atomic */
                AtomicCmd *ac = group->commands[0];
                size_t len = 0;
                for(int ai = 0; ac->argv[ai]; ai++)
                    len += strlen(ac->argv[ai]) + 1;
                char *job_name = malloc(len + 2);
                if(!job_name) {
                    perror("malloc");
                    exit(EXIT_FAILURE);
                }
                job_name[0] = '\0';
                for(int ai = 0; ac->argv[ai]; ai++) {
                    strcat(job_name, ac->argv[ai]);
                    if(ac->argv[ai + 1])
                        strcat(job_name, " ");
                }
                backgroundJobs[jobCnt] = initialiseJob(f, nextJobID, job_name);
                free(job_name);
                printf("[%d] %d\n", nextJobID, f);
                jobCnt++;
                nextJobID++;
            }
        } else {
            if(executeCmdGroup(group) == false) {
                return false;
            }
        }
        i++;
    }
    return true;
}

bool executeHop(AtomicCmd *cmd) {
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
        char *symbol = cmd->argv[i];
        char buffer[MID_PATH_MAX];

        // FIX: Get the current directory *before* any changes are made.
        if(getcwd(buffer, sizeof(buffer)) == NULL) {
            printf("Failure to fetch Dir\n");
            return false;
        }

        if(!strcmp(symbol, "-")) {
            if(prevDir == NULL) {
                //            printf("OLDPWD not set\n");
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

bool executeReveal(AtomicCmd *cmd) {
    bool flagA = false;
    bool flagL = false;
    char *targetPath = NULL;

    int i = 1;
    while(cmd->argv[i] != NULL) {
        char *symbol = cmd->argv[i];
        if(symbol[0] == '-' && strlen(symbol) != (size_t)(1)) {
            // check for 'a' and 'l' flags
            if(strchr(symbol, 'a'))
                flagA = true;
            if(strchr(symbol, 'l'))
                flagL = true;
        } else {
            // Check for multiple arguments for reveal
            if(targetPath != NULL) {
                fprintf(stderr, "reveal: Invalid Syntax!\n");
                return false;
            }
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
            printf("No such directory!\n");
            return false;
        }
        targetPath = prevDir;
    }

    DIR *targetDir = opendir(targetPath);
    if(targetDir == NULL) {
        printf("No such directory!\n");
        return false;
    }

    struct dirent *dirEntry;
    int maxFileCnt = 20;
    int cnt = 0;
    char **files = (char **)malloc(sizeof(char *) * maxFileCnt);
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
            files = realloc(files, sizeof(char *) * maxFileCnt);
        }
        files[cnt] = strdup(dirEntry->d_name);
        if(files[cnt] == NULL) {
            printf("Memory allocation error\n");
            exit(EXIT_FAILURE);
        }
        cnt++;
    }

    qsort(files, cnt, sizeof(char *), compareStrings);

    if(flagL) {
        for(int i = 0; i < cnt; i++)
            printf("%s\n", files[i]);
    } else {
        for(int i = 0; i < cnt; i++) {
            printf("%s ", files[i]);
            if(i == cnt - 1)
                printf("\n");
        }
    }

    for(int i = 0; i < cnt; i++)
        free(files[i]);
    free(files);

    closedir(targetDir);
    return true;
}
// In src/execute.c, place this function before initializeHistory.

// This function loads the command history from the persistent file into memory.
void loadHistory() {
    FILE *file = fopen(cmdHistoryFile, "r");
    if(file == NULL) {
        return;
    }

    char line_buffer[PATH_MAX]; // A buffer to read each line into.

    // Read the file line by line.
    while(fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        // Remove the trailing newline character that fgets reads.
        line_buffer[strcspn(line_buffer, "\n")] = '\0';

        // Don't add empty lines from the history file.
        if(strlen(line_buffer) > 0) {
            addCmd(line_buffer);
        }
    }

    fclose(file);
}

void initializeHistory() {
    if(shell_pid == 0) {
        shell_pid = getpid();
    }
    cmdHistoryFile = (char *)malloc(sizeof(char) * PATH_MAX);
    snprintf(cmdHistoryFile, PATH_MAX, "%s/.myshell_history", homeDirectory);
    loadHistory();
}

// This function writes the contents of the in-memory log to the persistent file.
void saveToLogFile() {
    // Open the file in write mode, which overwrites it completely.
    FILE *file = fopen(cmdHistoryFile, "w");
    if(file == NULL) {
        return;
    }

    // Correctly loop through the circular buffer from oldest to newest.
    int start_index = (logCnt == MAX_HISTORY) ? nextLog : 0;
    for(int i = 0; i < logCnt; i++) {
        int current_index = (start_index + i) % MAX_HISTORY;
        fprintf(file, "%s\n", cmdLog[current_index]);
    }

    fclose(file);
}

// Helper function to add a command to our in-memory history log.
// It handles the circular buffer logic, the max command limit, and duplicate prevention.
void addCmd(const char *command) {
    // Do not store any shell_cmd if the command is "log" itself.
    if(strncmp(command, "log", 3) == 0 && (command[3] == ' ' || command[3] == '\0')) {
        return;
    }

    // Do not store a command if it is identical to the previously executed one.
    if(logCnt > 0) {
        // Calculate the index of the most recently added command in the circular buffer.
        int last_cmd_index = (nextLog - 1 + MAX_HISTORY) % MAX_HISTORY;
        if(strcmp(cmdLog[last_cmd_index], command) == 0) {
            return; // It's a duplicate, so we do nothing.
        }
    }

    // If the buffer is full and we're about to overwrite an old command,
    // we must free the memory of that old command string first to prevent a leak.
    if(cmdLog[nextLog] != NULL) {
        free(cmdLog[nextLog]);
    }

    // Store a permanent copy of the new command string.
    cmdLog[nextLog] = strdup(command);

    // Move the "next slot" pointer, wrapping around if necessary.
    nextLog = (nextLog + 1) % MAX_HISTORY;

    // The history_count should not exceed the maximum.
    if(logCnt < MAX_HISTORY) {
        logCnt++;
    }
}

bool executeLog(AtomicCmd *cmd) {
    if(cmd->argv[1] == NULL) {
        int start_index = (logCnt == MAX_HISTORY) ? nextLog : 0;
        for(int i = 0; i < logCnt; i++) {
            int current_index = (start_index + i) % MAX_HISTORY;
            printf("%s\n", cmdLog[current_index]);
        }
    } else if(!strcmp(cmd->argv[1], "purge")) {
        for(int i = 0; i < logCnt; i++) {
            free(cmdLog[i]);
            cmdLog[i] = NULL;
        }
        logCnt = 0;
        nextLog = 0;
    } else if(!strcmp(cmd->argv[1], "execute")) {
        if(cmd->argv[2] == NULL) {
            fprintf(stderr, "log: requires index\n");
            return false;
        }
        int userIndex = atoi(cmd->argv[2]);
        if(userIndex <= 0 || userIndex > logCnt) {
            fprintf(stderr, "log: invalid index\n");
            return false;
        }
        /* HIGHLIGHT: Index 1 = most recent */
        int start_index = (logCnt == MAX_HISTORY) ? nextLog : 0;
        int newest_index = (start_index + logCnt - 1) % MAX_HISTORY;
        int physical_index = (newest_index - (userIndex - 1) + MAX_HISTORY) % MAX_HISTORY;

        char *commandInLog = cmdLog[physical_index];
        if(!commandInLog) {
            fprintf(stderr, "log: invalid index\n");
            return false;
        }
        ShellCmd *parsedCmd = parseCommand(commandInLog);
        if(parsedCmd) {
            bool in_child = (shell_pid != 0 && getpid() != shell_pid);
            if(in_child) {
                /* Optimized child execution path:
                   If the recalled command is a single foreground atomic command
                   (no pipelines, no sequencing, not background),
                   execute it directly via runCommand to preserve pipeline FDs.
                   Otherwise fall back to /bin/sh -c. */
                if(parsedCmd->groupCount == 1) {
                    CmdGroup *g = parsedCmd->groups[0];
                    if(!g->background && g->cmdCount == 1) {
                        AtomicCmd *only = g->commands[0];
                        runCommand(only); /* does not return on success */
                        freeShellCmd(parsedCmd);
                        _exit(127); /* runCommand failed/returned */
                    }
                }
                freeShellCmd(parsedCmd);
                execl("/bin/sh", "sh", "-c", commandInLog, (char *)NULL);
                fprintf(stderr, "log: exec failed\n");
                _exit(127);
            } else {
                executeCommand(parsedCmd, commandInLog);
                freeShellCmd(parsedCmd);
            }
        }
    }
    return true;
}
 
/* ping builtin: Syntax: ping <pid> <signal_number>
   Behavior:
     - actual_signal = signal_number % 32
     - On success: "Sent signal signal_number to process with pid <pid>"
     - If pid invalid / process not found: "No such process found"
     - On invalid syntax (missing args, extra args, non-numeric): "Invalid syntax!"
*/
bool executePing(AtomicCmd *cmd) {
    if(!cmd->argv[1] || !cmd->argv[2] || cmd->argv[3]) {
        printf("Invalid syntax!\n");
        return false;
    }
    char *endp1 = NULL;
    char *endp2 = NULL;
    long pid_l = strtol(cmd->argv[1], &endp1, 10);
    long sig_l = strtol(cmd->argv[2], &endp2, 10);
    if(*endp1 != '\0' || *endp2 != '\0') {
        printf("Invalid syntax!\n");
        return false;
    }
    if(pid_l <= 0) {
        printf("No such process found\n");
        return false;
    }
    int original_sig = (int)sig_l;
    int actual_sig = original_sig % 32;
    if(kill((pid_t)pid_l, actual_sig) == -1) {
        printf("No such process found\n");
        return false;
    }
    printf("Sent signal %d to process with pid %ld\n", original_sig, pid_l);
    return true;
}
 
/* HIGHLIGHT: fg implementation */
bool executeFg(AtomicCmd *cmd) {
    Job *target = NULL;
    if(cmd->argv[1]) {
        int id = atoi(cmd->argv[1]);
        target = findJobByID(id);
    } else {
        target = mostRecentJob();
    }
    if(!target) {
        printf("No such job\n");
        return false;
    }
    foreground_pgid = target->pid;
    if(target->status == STOPPED) {
        kill(-target->pid, SIGCONT);
    }
    /* Print full command */
    printf("%s\n", target->cmdName);
    int status;
    if(waitpid(target->pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }
    if(WIFSTOPPED(status)) {
        target->status = STOPPED;
        printf("[%d] Stopped %s\n", target->jobID, target->cmdName);
    } else {
        for(int i = 0; i < jobCnt; i++) {
            if(backgroundJobs[i] == target) {
                removeJob(i);
                break;
            }
        }
    }
    foreground_pgid = 0;
    tcsetpgrp(STDIN_FILENO, getpgrp());
    return true;
}

/* HIGHLIGHT: bg implementation */
bool executeBg(AtomicCmd *cmd) {
    Job *target = NULL;
    if(cmd->argv[1]) {
        int id = atoi(cmd->argv[1]);
        target = findJobByID(id);
    } else {
        target = mostRecentJob();
    }
    if(!target) {
        printf("No such job\n");
        return false;
    }
    if(target->status == RUNNING) {
        printf("Job already running\n");
        return false;
    }
    kill(-target->pid, SIGCONT);
    target->status = RUNNING;
    printf("[%d] %s &\n", target->jobID, target->cmdName);
    return true;
}

// This function implements the 'activities' built-in command.
bool executeActivities(AtomicCmd *cmd) {
    // Step 1: First, reap any jobs that have already terminated.
    // This cleans the list and prints their final status.
    checkBackgroundJobs();

    // Step 2: Handle the case where there are no active jobs left.
    if(jobCnt == 0) {
        return true; // Nothing to display.
    }

    // Step 3: Create a temporary list to hold the formatted strings for sorting.
    char **displayList = malloc(sizeof(char *) * jobCnt);
    if(displayList == NULL) {
        perror("malloc");
        return false;
    }

    // Step 4: Populate the display list with formatted strings.
    for(int i = 0; i < jobCnt; i++) {
        // Determine the status string based on the job's state.
        const char *status_str = (backgroundJobs[i]->status == RUNNING) ? "Running" : "Stopped";

        // Allocate space for the formatted string.
        // A large fixed size is safe here.
        char buffer[1024];
        /* HIGHLIGHT: display jobID instead of raw pid */
        sprintf(buffer, "[%d] : %s - %s", backgroundJobs[i]->jobID, backgroundJobs[i]->cmdName, status_str);

        // Store a permanent copy in our display list.
        displayList[i] = strdup(buffer);
    }

    // Step 5: Sort the list lexicographically using the existing compareStrings function.
    qsort(displayList, jobCnt, sizeof(char *), compareStrings);

    // Step 6: Print the sorted list.
    for(int i = 0; i < jobCnt; i++) {
        printf("%s\n", displayList[i]);
    }

    // Step 7: Clean up all the memory we allocated for the display list.
    for(int i = 0; i < jobCnt; i++) {
        free(displayList[i]);
    }
    free(displayList);

    return true;
}
