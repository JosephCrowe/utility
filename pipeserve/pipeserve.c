/*******************************************************************************
 *  pipeserv accepts connections to a UNIX domain socket in the file system,
 *  forwarding each line of its stdin to each connected client, and each line
 *  received from any connected client to its stdout.
 *
 *  General information is printed to stderr.
 *
 *  Lines longer than BUFFER may be transmitted non-atomically, and hence mixed
 *  with other data.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "list.h"


#define UNIX_PATH_MAX 108
#define LAMBDA(TYPE, BODY) ({ TYPE __lambda__ BODY __lambda__; })

#define SIGRT_IO SIGRTMIN
#define BACKLOG 16
#define BUFFER 10

static int check(int);
static ssize_t scheck(ssize_t);

static void setup_io(int, int);
static void handle_io(int, siginfo_t *, void *);

static void server_io(void);
static void client_io(int);
static void stdin_io(void);
static void child_io(int);

char *name;
char *socket_name;

int server;
list_t *clients;

int child_in;
int child_out;
int child_err;

char stdin_buffer[BUFFER];
size_t stdin_bytes;

int main(int argc, char **argv, char *envp[]) {
    /* Read command-line arguments. */
    char *_name = basename(argv[0]);
    name = malloc(strlen(_name));
    strcpy(name, _name);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s SOCKET_NAME COMMAND...\n", name);
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) > UNIX_PATH_MAX) {
        fprintf(stderr, "Error: SOCKET_NAME is too long.\n");
        return EXIT_FAILURE;
    }

    socket_name = argv[1];

    /* Start child process. */
    int child_stdin[2], child_stdout[2], child_stderr[2];
    check(pipe(child_stdin));
    check(pipe(child_stdout));
    check(pipe(child_stderr));

    int child = check(fork());
    if (child == 0) {
        check(dup2(STDIN_FILENO, child_stdin[0]));
        check(dup2(STDOUT_FILENO, child_stdout[1]));
        check(dup2(STDERR_FILENO, child_stderr[1]));
        check(execvpe(argv[2], &argv[2], envp));
    }

    child_in = child_stdin[1];
    child_out = child_stdout[0];
    child_err = child_stderr[0];

    /* Set up signal handlers. */
    sigset_t empty;
    sigemptyset(&empty);

    struct sigaction action;
    action.sa_mask = empty;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = &handle_io;
    sigaction(SIGRT_IO, &action, NULL);

    action.sa_flags = 0;
    action.sa_handler = &exit;
    sigaction(SIGINT, &action, NULL);

    /* Set up listening socket. */
    clients = LIST_EMPTY;

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_name);

    server = check(socket(AF_UNIX, SOCK_STREAM, 0));
    check(bind(server, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)));
    atexit(LAMBDA(void, (void) { unlink(socket_name); }));
    setup_io(server, O_NONBLOCK);
    check(listen(server, BACKLOG));

    /* Set up stdin. */
    stdin_bytes = 0;
    setup_io(STDIN_FILENO, O_NONBLOCK);

    /* Wait for signals indefinitely. */
    for (;;) sigsuspend(&empty);
}

void setup_io(int fd, int flags) {
    check(fcntl(fd, F_SETSIG, SIGRT_IO));
    check(fcntl(fd, F_SETFL, O_ASYNC | flags));
    check(fcntl(fd, F_SETOWN, getpid()));
}

void handle_io(int signum, siginfo_t *info, void *context) {
    int fd = info->si_fd;

    if (fd == STDIN_FILENO) return stdin_io();
    if (fd == server) return server_io();
    if (fd == child_out || fd == child_err) return child_io(fd);

    for (list_t *item = clients; item != NULL; item = item->tail)
        if (fd == item->head) return client_io(fd);
}

void server_io (void) {
    for (;;) {
        int client = accept(server, NULL, 0);
        if (client == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        scheck(client);
        setup_io(client, 0);
        clients = list_add(clients, client);
        fprintf(stderr, "%s: connect(%d)\n", name, client);
    }
}

void client_io (int client) {
    for (;;) {
        char buffer[BUFFER];

        ssize_t bytes = recv(
            client, (void *) &buffer, BUFFER, MSG_PEEK | MSG_DONTWAIT);
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (bytes == 0 || bytes == -1) {
            fprintf(stderr, "%s: disconnect(%d)\n", name, client);
            close(client);
            clients = list_del(clients, client);
            break;
        }

        char *end = (char *) memchr(buffer, '\n', bytes);
        if (end != NULL) bytes = end - buffer + 1;
        else if (bytes < BUFFER) continue;

        scheck(recv(client, (void *) buffer, bytes, 0));

        fprintf(stderr, "%s(%d): ", name, client);
        write(STDERR_FILENO, (void *) buffer, bytes);
        if (buffer[bytes - 1] != '\n') fprintf(stderr, "\\\n");

        write(STDOUT_FILENO, (void *) buffer, bytes);
    }
}

void stdin_io(void) {
    for (;;) {
        size_t remain = BUFFER - stdin_bytes;
        ssize_t bytes = read(STDIN_FILENO, (void *) &stdin_buffer + stdin_bytes,
                             remain);
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        stdin_bytes += scheck(bytes);

        char *end = (char *) memchr(&stdin_buffer, '\n', stdin_bytes);
        if (end != NULL) bytes = end - stdin_buffer + 1;
        else if (stdin_bytes == BUFFER) bytes = BUFFER;
        else continue;

        for (list_t *item = clients; item != NULL; item = item->tail) {
            int client = item->head;
            scheck(send(client, (void *) &stdin_buffer, bytes, 0));
        }

        memmove(stdin_buffer, stdin_buffer + bytes, stdin_bytes - bytes);
        stdin_bytes -= bytes;
    }
}

void child_io(int fd) {
    /* fd is child_out or child_err. */
    
}

static int check (int result) {
    if (result == -1) err(EXIT_FAILURE, NULL);
    return result;
}

static ssize_t scheck (ssize_t result) {
    if (result == -1) err (EXIT_FAILURE, NULL);
    return result;
}
