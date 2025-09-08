#ifndef EXECUTE_H_
#define EXECUTE_H_

#include "shell.h"
#include "parser.h"

#define MID_PATH_MAX 1000
#define maxBackgroundCnt 100
#define MAX_HISTORY 15

void initializeJobControl();
bool executeCommand(ShellCmd * cmd, char * command); 
bool executeHop(AtomicCmd * cmd);
bool executeReveal(AtomicCmd * cmd);
bool executeLog(AtomicCmd * cmd);
bool executeArbitaryCommands(AtomicCmd * cmd);
void runCommand(AtomicCmd * cmd);
void checkBackgroundJobs();
void addCmd(const char * command);
void saveToLogFile();
void loadHistory();
#endif 
