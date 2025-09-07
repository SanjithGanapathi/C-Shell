#ifndef SHELL_H_
#define SHELL_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<ctype.h>
#include<stdbool.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/utsname.h>
#include<dirent.h>
#include<sys/stat.h>
#include<fcntl.h>

typedef struct {
    pid_t pid;          
    int jobID;         
    char* cmdName; 
} Job;

extern char * homeDirectory; // This is to declare variables completely global
extern char * prevDir; // This is to declare variables completely global
extern int nextJobID; // This is to declare the next job ID global
extern int jobCapacity; // This is to declare the next job ID global
#endif 
