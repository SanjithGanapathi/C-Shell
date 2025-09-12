#include "../include/shell.h"
#include "../include/parser.h"

// ... (tokenizeInput, freeTokens, and all free... functions are correct) ...
// ... (parseAtomic and parseCmdGroup are also correct) ...

void freeTokens(Token *tokens) {
    if(!tokens) return;
    for(int i = 0; tokens[i].type != TOK_END; i++) {
        free(tokens[i].value);
    }
    free(tokens);
}

void printTokens(Token * toks) {
    int i = 0;
    while(toks[i].type != TOK_END) {
        printf("%d", toks[i].type);
        if(toks[i].type == TOK_NAME) {
            printf(" %s", toks[i].value);
        }
        printf("\n");
        i++;
    }
}

Token *tokenizeInput(const char *input) {
    int maxTokencnt = INIT_TOK_SIZE;
    Token *tokens = (Token*)malloc(sizeof(Token) * maxTokencnt);
    if(!tokens) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    int i = 0, tokcnt = 0;
    int len = strlen(input);

    while(i<len) {
        if(tokcnt >= maxTokencnt - 1) {
            maxTokencnt *= 2;
            tokens = (Token*)realloc(tokens, sizeof(Token) * maxTokencnt);
            if(tokens == NULL) {
                fprintf(stderr, "Realloc Failure\nExiting...\n");
                exit(EXIT_FAILURE);
            }
        }

        if(isspace(input[i])) {
            i++;
            continue;
        }

        if(input[i] == '|') {
            tokens[tokcnt++] = (Token){TOK_PIPE, NULL};
            i++;
        } else if(input[i] == '&') {
            tokens[tokcnt++] = (Token){TOK_AND, NULL};
            i++;
        } else if(input[i] == ';') {
            tokens[tokcnt++] = (Token){TOK_SEMCOL, NULL};
            i++;
        } else if(input[i] == '<') {
            tokens[tokcnt++] = (Token){TOK_INPUT, NULL};
            i++;
        } else if(input[i] == '>') {
            if(i + 1 < len && input[i + 1] == '>') {
                tokens[tokcnt++] = (Token){TOK_APPEND, NULL};
                i += 2;
            } else {
                tokens[tokcnt++] = (Token){TOK_OUTPUT, NULL};
                i++;
            }
        } else {
            int start = i;
            while(i<len && !isspace(input[i]) && !strchr("|&;<>", input[i])) {
                i++;
            }
            if(i > start) {
                char *value = strndup(&input[start], i - start);
                tokens[tokcnt++] = (Token){TOK_NAME, value};
            }
        }
    }

    tokens[tokcnt] = (Token){TOK_END, NULL};
    return tokens;
}


void freeAtomicCmd(AtomicCmd *cmd) {
    if(!cmd) return;
    if(cmd->argv) {
        for (int i = 0; cmd->argv[i] != NULL; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }
    // Free legacy single redirection targets
    free(cmd->inputFile);
    free(cmd->outputFile);
    // HIGHLIGHT: Free extended redirection list
    if(cmd->redirs) {
        for(int i = 0; i < cmd->redirCount; i++) {
            free(cmd->redirs[i].path);
        }
        free(cmd->redirs);
    }
    free(cmd);
}

void freeCmdGroup(CmdGroup *group) {
    if(!group) return;
    for (int i = 0; i < group->cmdCount; i++) {
        freeAtomicCmd(group->commands[i]);
    }
    free(group->commands);
    free(group);
}

void freeShellCmd(ShellCmd *shell_cmd) {
    if(!shell_cmd) return;
    for (int i = 0; i < shell_cmd->groupCount; i++) {
        freeCmdGroup(shell_cmd->groups[i]);
    }
    free(shell_cmd->groups);
    free(shell_cmd);
}

AtomicCmd * parseAtomic(Token * toks, int * i) {
    if(toks[*i].type != TOK_NAME) {
        return NULL;
    }

    int maxArgcnt = INIT_ARG_SIZE;
    AtomicCmd * cmd = (AtomicCmd*)calloc(1, sizeof(AtomicCmd));
    if(!cmd) return NULL;
    cmd->argv = malloc(maxArgcnt*sizeof(char*));
    if(!cmd->argv) {
        free(cmd);
        return NULL;
    }
    cmd->redirCap = 4;
    cmd->redirs = calloc(cmd->redirCap, sizeof(Redirection));
    cmd->redirCount = 0;

    int argc = 0;

    while(toks[*i].type != TOK_PIPE &&
          toks[*i].type != TOK_AND &&
          toks[*i].type != TOK_SEMCOL &&
          toks[*i].type != TOK_END) {

        if(toks[*i].type == TOK_NAME) {
            if(argc == maxArgcnt-1) {
                maxArgcnt *= 2;
                char **tmp = realloc(cmd->argv, sizeof(char*)*maxArgcnt);
                if(tmp == NULL) {
                    fprintf(stderr, "Realloc Error..\n");
                    freeAtomicCmd(cmd);
                    return NULL;
                }
                cmd->argv = tmp;
            }
            cmd->argv[argc++] = strdup(toks[*i].value);
            (*i)++;
            continue;
        }

        if(toks[*i].type == TOK_INPUT || toks[*i].type == TOK_OUTPUT || toks[*i].type == TOK_APPEND) {
            TokenType type = toks[*i].type;
            (*i)++;

            // Require a filename after redirection operator
            if(toks[*i].type != TOK_NAME) {
                freeAtomicCmd(cmd);
                return NULL;
            }

            // Expand redirection array if needed
            if(cmd->redirCount == cmd->redirCap) {
                cmd->redirCap *= 2;
                Redirection *rtemp = realloc(cmd->redirs, sizeof(Redirection)*cmd->redirCap);
                if(!rtemp) {
                    fprintf(stderr, "Realloc Error..\n");
                    freeAtomicCmd(cmd);
                    return NULL;
                }
                cmd->redirs = rtemp;
            }

            // Map token -> RedirKind
            RedirKind rk;
            if(type == TOK_INPUT) rk = REDIR_IN;
            else if(type == TOK_APPEND) rk = REDIR_OUT_APPEND;
            else rk = REDIR_OUT_TRUNC;

            cmd->redirs[cmd->redirCount].kind = rk;
            cmd->redirs[cmd->redirCount].path = strdup(toks[*i].value);
            cmd->redirCount++;

            // Maintain legacy single-field view (store the last one of each type)
            if(rk == REDIR_IN) {
                if(cmd->inputFile) free(cmd->inputFile);
                cmd->inputFile = strdup(toks[*i].value);
            } else {
                if(cmd->outputFile) free(cmd->outputFile);
                cmd->outputFile = strdup(toks[*i].value);
                cmd->append = (rk == REDIR_OUT_APPEND);
            }

            (*i)++;
            continue;
        }

        // Any other token type here is a syntax error for an atomic command
        freeAtomicCmd(cmd);
        return NULL;
    }

    cmd->argv[argc] = NULL;
    return cmd;
}

CmdGroup *parseCmdGroup(Token *toks, int *i) {
    CmdGroup * group = (CmdGroup*)calloc(1, sizeof(CmdGroup));
    int maxCmdCnt = INIT_CMD_SIZE;
    group->commands = malloc(sizeof(AtomicCmd *) * maxCmdCnt);

    AtomicCmd * initialAtomic = parseAtomic(toks, i);
    if(initialAtomic == NULL) {
        free(group->commands);
        free(group);
        return NULL;
    }
    group->commands[(group->cmdCount)++] = initialAtomic;

    while(toks[*i].type == TOK_PIPE) {
        (*i)++;
        AtomicCmd * nextAtomic = parseAtomic(toks, i);
        if(nextAtomic == NULL) {
            freeCmdGroup(group);
            return NULL;
        }
        if(group->cmdCount >= maxCmdCnt) {
            maxCmdCnt *= 2;
            group->commands = realloc(group->commands, sizeof(AtomicCmd*) * maxCmdCnt);
            if(group->commands == NULL) {
                printf("Realloc Failure\n");
                exit(EXIT_FAILURE);
            }
        }
        group->commands[(group->cmdCount)++] = nextAtomic;
    }
    return group;
}

ShellCmd * parseCommand(const char * input) {
    Token * toks = tokenizeInput(input);
    if(toks == NULL) {
        return NULL;
    }
    int i = 0;

    ShellCmd * cmd = (ShellCmd*)calloc(1, sizeof(ShellCmd));
    int maxGroupCnt = INIT_GRP_SIZE;
    cmd->groups = malloc(sizeof(CmdGroup*) * maxGroupCnt);

    if(cmd->groups == NULL) {
        free(cmd);
        freeTokens(toks);
        exit(EXIT_FAILURE);
    }
    
    CmdGroup * currentGroup = parseCmdGroup(toks, &i);
    if(currentGroup == NULL) {
        free(cmd->groups);
        free(cmd);
        freeTokens(toks);
        return NULL;
    }
    cmd->groups[(cmd->groupCount)++] = currentGroup;

    // This loop parses a sequence of command groups separated by ';' or '&'.
    while(toks[i].type == TOK_AND || toks[i].type == TOK_SEMCOL) {

        if(toks[i].type == TOK_AND) {
            cmd->groups[cmd->groupCount-1]->background = true;
        }

        i++; // Consume the TOK_AND || TOK_SEMCOL

        if(toks[i].type == TOK_END) {
            break;
        }

        CmdGroup * currentCmd = parseCmdGroup(toks, &i);
        if(currentCmd == NULL) {
            freeShellCmd(cmd);
            freeTokens(toks);
            return NULL;
        }

        if(cmd->groupCount >= maxGroupCnt) {
            maxGroupCnt *= 2;
            cmd->groups = realloc(cmd->groups, sizeof(CmdGroup*) * maxGroupCnt);
            if(cmd->groups == NULL) {
                printf("Realloc Failure\n");
                exit(EXIT_FAILURE);
            }
        }
        cmd->groups[(cmd->groupCount)++] = currentCmd;
    }

    if (toks[i].type != TOK_END) {
        printf("Syntax Error: Expected ';' or '&' between commands.\n");
        freeShellCmd(cmd);
        freeTokens(toks);
        return NULL;
    }

    freeTokens(toks);
    return cmd;
}
