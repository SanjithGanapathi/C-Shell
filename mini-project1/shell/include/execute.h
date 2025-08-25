#ifndef EXECUTE_H_
#define EXECUTE_H_

#include "shell.h"
#include "parser.h"

#define MID_PATH_MAX 1000


bool executeCommand(ShellCmd * cmd); 
bool executeHop(AtomicCmd * cmd);
bool executeReveal(AtomicCmd * cmd);
bool executeLog(AtomicCmd * cmd);
bool executeArbitaryCommands(AtomicCmd * cmd);

#endif 

