/*******************************************************************************
 *  pipeserv: accepts connections on a UNIX domain socket, forwarding each
 *            line read from stdin to each connected client, and each line read
 *            from a connected client to stdout.
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

#define BACKLOG 16
#define BUFFER 512
#define SIGRT_SERVER  (SIGRTMIN + 0)
#define SIGRT_CLIENT  (SIGRTMIN + 1)
#define SIGRT_STDIN   (SIGRTMIN + 2)

static int check(int);
static ssize_t scheck(ssize_t);
void atexit_func(void);
void sigint(int, siginfo_t *, void *);
void sigrt_server(int, siginfo_t *, void *);
void sigrt_client(int, siginfo_t *, void *);
void sigrt_stdin(int, siginfo_t *, void *);

char *socket_name;
list_t *clients;

int main(int argc, char **argv) {
    /* Read command-line arguments. */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s SOCKET_NAME\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    if (strlen(argv[1]) > UNIX_PATH_MAX) {
        fprintf(stderr, "Error: SOCKET_NAME is too long.\n");
        return EXIT_FAILURE;
    }
    socket_name = argv[1];

    /* Set up signal handlers. */
    sigset_t empty;
    sigemptyset(&empty);

    struct sigaction action;
    action.sa_mask = empty;
    action.sa_flags = SA_SIGINFO;

    action.sa_sigaction = &sigint;
    sigaction(SIGINT, &action, NULL);

    action.sa_sigaction = &sigrt_server;
    sigaction(SIGRT_SERVER, &action, NULL);

    action.sa_sigaction = &sigrt_client;
    sigaction(SIGRT_CLIENT, &action, NULL);

    action.sa_sigaction = &sigrt_stdin;
    sigaction(SIGRT_STDIN, &action, NULL);

    /* Set up listening socket. */
    clients = LIST_EMPTY;

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_name);

    int server = check(socket(AF_UNIX, SOCK_STREAM, 0));
    check(bind(server, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)));
    atexit(&atexit_func);

    check(fcntl(server, F_SETSIG, SIGRT_SERVER));
    check(fcntl(server, F_SETFL, O_ASYNC | O_NONBLOCK));
    check(fcntl(server, F_SETOWN, getpid()));
    check(listen(server, BACKLOG));

    /* Set up stdin. */
    check(fcntl(STDIN_FILENO, F_SETSIG, SIGRT_STDIN));
    check(fcntl(STDIN_FILENO, F_SETFL, O_ASYNC | O_NONBLOCK));
    check(fcntl(STDIN_FILENO, F_SETOWN, getpid()));

    /* Wait for signals indefinitely. */
    for (;;) sigsuspend(&empty);
}

void sigrt_server (int signum, siginfo_t *info, void *context) {
    int server = info->si_fd;
    for (;;) {
        int client = accept(server, NULL, 0);
        if (client == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        scheck(client);

        check(fcntl(client, F_SETSIG, SIGRT_CLIENT));
        check(fcntl(client, F_SETFL, O_ASYNC));
        check(fcntl(client, F_SETOWN, getpid()));

        clients = list_add(clients, client);
        warnx("connect: %d", client);
    }
}

void sigrt_client (int signum, siginfo_t *info, void *context) {
    int client = info->si_fd;

    list_t *item;
    for (item = clients; item != NULL; item = item->tail)
        if (item->head == client) break;
    if (item == NULL) return;

    for (;;) {
        char buffer[BUFFER];

        ssize_t bytes = recv(client, (void *) &buffer, BUFFER,
                             MSG_PEEK | MSG_DONTWAIT);
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (bytes == 0 || bytes == -1) {
            warnx("disconnect: %d", client);
            close(client);
            clients = list_del(clients, client);
            break;
        }

        char *end = (char *) memchr(buffer, '\n', bytes);
        if (end != NULL) {
            /* Newline found. */
            scheck(recv(client, (void *) buffer, bytes, 0));
        } else if (bytes == BUFFER) {
            /* Buffer is full, but no newline. */
            buffer[bytes - 1] = '\n';
        } else {
            /* Buffer is not full, and no newline. */
            continue;
        }
        scheck(write(STDOUT_FILENO, (void *) buffer, bytes));
    }
}

void sigrt_stdin(int signum, siginfo_t *info, void *context) {
    for (;;) {
        char buffer[BUFFER];
        ssize_t bytes = read(STDIN_FILENO, (void *) &buffer, BUFFER);
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        scheck(bytes);

        for (list_t *item = clients; item != NULL; item = item->tail) {
            int client = item->head;
            scheck(send(client, (void *) &buffer, bytes, 0));
        }
    }
}

void sigint (int signum, siginfo_t *info, void *context) {
    exit(EXIT_SUCCESS);
}

static int check (int result) {
    if (result == -1) err(EXIT_FAILURE, NULL);
    return result;
}

static ssize_t scheck (ssize_t result) {
    if (result == -1) err (EXIT_FAILURE, NULL);
    return result;
}

void atexit_func(void) {
    unlink(socket_name);
}
