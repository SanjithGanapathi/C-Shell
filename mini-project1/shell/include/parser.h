#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

// --- Defines for Initial Allocation Sizes ---
#define MAX_LINE_LENGTH 1024
#define INIT_TOK_SIZE 50
#define INIT_ARG_SIZE 8
#define INIT_CMD_SIZE 4
#define INIT_GRP_SIZE 2

// Token types
typedef enum {
    TOK_NAME,     // command/filename
    TOK_PIPE,     // |
    TOK_AND,      // &
    TOK_SEMCOL,   // ;
    TOK_INPUT,    // <
    TOK_OUTPUT,   // >
    TOK_APPEND,   // >>
    TOK_END       // end of input
} TokenType;

// Token struct
typedef struct {
    TokenType type;
    char *value;  // only used if type == TOK_NAME
} Token;

// Represents an atomic command like `cat file.txt > out.txt`
typedef struct AtomicCmd {
    char **argv;         // arguments: ["cat","file.txt",NULL]
    char *inputFile;    // "< file"
    char *outputFile;   // "> file" or ">> file"
    bool append;         // true if >>
} AtomicCmd;

// Represents a command group: pipeline of atomics (cmd1 | cmd2 | cmd3)
typedef struct CmdGroup {
    AtomicCmd **commands;
    int cmdCount;
    bool background;     // trailing `&`
} CmdGroup;

// Represents a full shell command: may have multiple groups, bg, &&, etc.
typedef struct ShellCmd {
    CmdGroup **groups;
    int groupCount;
} ShellCmd;

// tokenizing functions
Token *tokenizeInput(const char *input);
void freeTokens(Token *tokens);
void printTokens(Token *tokens);

// free commands for groups
void freeAtomicCmd(AtomicCmd * cmd);
void freeCmdGroup(CmdGroup * cmd);
void freeShellCmd(ShellCmd * cmd);

// parsing functions
ShellCmd * parseCommand(const char * input);
CmdGroup * parseCmdGrp(Token *toks, int *i);
AtomicCmd * parseAtomic(Token *toks, int *i);

#endif // PARSER_H

