#define _GNU_SOURCE

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "buffer.h"
#include "list.h"

typedef struct {
    int fd;
    char data[BUFFER_SIZE];
    char *end;
    size_t shift;
} buffer_t;

static plist *buffers = NULL;

ssize_t buffer_readline(int fd, char **line) {
    return buffer_readline_r(fd, line, NULL, NULL);
}

ssize_t buffer_readline_r(int fd, char **line, size_t *rsize, char **rdata ) {
    buffer_t *buffer = NULL;
    for (plist *item = buffers; item != NULL; item = item->tail) {
        buffer_t *itemb = (buffer_t *) item->head;
        if (itemb->fd == fd) {
            buffer = itemb;
            break;
        }
    }
    if (buffer == NULL) {
        buffer = malloc(sizeof(buffer_t));
        buffer->fd = fd;
        buffer->end = &buffer->data[0];
        buffer->shift = 0;
        buffers = plist_add(buffers, buffer);
    }

    if (buffer->shift) {
        memmove(&buffer->data[0], &buffer->data[buffer->shift],
                buffer->end - &buffer->data[buffer->shift]);
        buffer->end -= buffer->shift;
        buffer->shift = 0;
    }

    char *start = buffer->end;
    ssize_t bytes;

    for (;;) {
        size_t space = BUFFER_SIZE - (buffer->end - &buffer->data[0]);
        bytes = read(buffer->fd, buffer->end, space);
        if (bytes == -1 || bytes == 0) break;
        char *endl = memchr(buffer->end, '\n', bytes);
        buffer->end += bytes;

        if (endl == NULL && bytes < space) continue;

        bytes = (endl == NULL) ? BUFFER_SIZE : (endl + 1 - &buffer->data[0]);
        buffer->shift = bytes;
        break;
    }

    if (bytes != -1) *line = &buffer->data[0];
    if (rdata != NULL) *rdata = start;
    if (rsize != NULL) *rsize = buffer->end - start;

    return bytes;
}

void buffer_clear(int fd) {
    plist head = { NULL, buffers };
    for (plist *prev = &head; prev->tail != NULL; prev = prev->tail) {
        buffer_t *buffer = prev->tail->head;
        if (buffer->fd != fd) continue;
        prev->tail = prev->tail->tail;
        free(buffer);
    }
    buffers = head.tail;
}
