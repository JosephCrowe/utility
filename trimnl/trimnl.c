#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const int BLOCK_SIZE = 1024;

void* safe(void* ptr) {
    if (ptr != NULL) return ptr;
    perror("perror");
    exit(EXIT_FAILURE);
}

void read_line(char** buf, size_t* buf_s, size_t* len) {
    *len = 0;
    **buf = '\0';
    for (;;) {
        fgets((*buf)+(*len), (*buf_s)-(*len), stdin);
        if (ferror(stdin)) safe(NULL);
        *len = strlen(*buf);
        if (feof(stdin) || (*buf)[(*len)-1] == '\n') break;
        *buf_s += BLOCK_SIZE;
        safe(realloc(*buf, *buf_s));
    }
}

int main(void) {
    size_t buf_s = BLOCK_SIZE;
    size_t len;
    char* buf = safe(malloc(buf_s));
    int first = 1;
    while (!feof(stdin)) {
        read_line(&buf, &buf_s, &len);
        if (!len) break;
        if (buf[len-1] == '\n') buf[len-1] = '\0';
        if (first) first = 0;
        else fputc('\n', stdout);
        fputs(buf, stdout);
    }
    fflush(stdout);
    return EXIT_SUCCESS;
}
