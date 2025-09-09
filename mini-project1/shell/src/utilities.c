#include "../include/utilities.h"

char * getSubstr(const char * str, int start, int len) {
    if(!str || start < 0 || len < 0 || (size_t)(start + len) > strlen(str)) {
        return NULL;
    }

    char * substr = (char*)malloc((len + 1) * sizeof(char));
    if(!substr) {
        perror("malloc failed");
        exit(1);
    }

    strncpy(substr, str + start, len);
    substr[len] = '\0';
    return substr;
}

int compareStrings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

