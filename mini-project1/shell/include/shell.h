#ifndef SHELL_H_
#define SHELL_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<unistd.h>
#include<ctype.h>
#include<stdbool.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/utsname.h>
#include<sys/types.h>
#include<dirent.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<limits.h>
#include<errno.h>

typedef enum {
    RUNNING,
    STOPPED
} JobStatus;

typedef struct {
    pid_t pid;        // process (and process group) id
    int jobID;        // shell-assigned job number
    char *cmdName;    // HIGHLIGHT: store full command string (not only argv[0])
    JobStatus status; // RUNNING / STOPPED
} Job;

extern Job **backgroundJobs;
extern int jobCnt;

extern char *homeDirectory;
extern char *prevDir;
extern int nextJobID;
extern int jobCapacity;
extern char *cmdHistoryFile; 
extern volatile pid_t foreground_pgid;
#endif
