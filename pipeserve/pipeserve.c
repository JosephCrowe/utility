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
#include <stdarg.h>

#include <err.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/prctl.h>

#include "list.h"
#include "buffer.h"

#define UNIX_PATH_MAX 108
#define LAMBDA(TYPE, BODY) ({ TYPE __lambda__ BODY __lambda__; })

#define SIGRT_IO SIGRTMIN
#define BACKLOG 16
#define BUFFER 10

static int check(int, char *);
static void report(const char *format, ...);

static void setup_io(int);
static void handle_io(int, siginfo_t *, void *);

static void server_io(void);
static void client_io(int);
static void stdin_io(void);
static void child_io(int);

static char *program_name;
static char *socket_name;
static int child_in, child_out, child_err;
static int server;
static ilist *clients = NULL;
static pid_t child;

int main(int argc, char **argv, char *envp[]) {
    /* Read command-line arguments. */
    char *name = basename(argv[0]);
    program_name = malloc(strlen(name));
    strcpy(program_name, name);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s SOCKET_NAME COMMAND...\n", program_name);
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) > UNIX_PATH_MAX) {
        report("Error: SOCKET_NAME is too long.\n");
        return EXIT_FAILURE;
    }

    socket_name = argv[1];

    /* Set up signal handlers. */
    sigset_t empty;
    sigemptyset(&empty);

    struct sigaction action;
    action.sa_mask = empty;

    action.sa_flags = SA_RESTART;
    action.sa_handler = &exit;
    sigaction(SIGINT, &action, NULL);

    action.sa_flags |= SA_SIGINFO;
    action.sa_sigaction = &handle_io;
    sigaction(SIGRT_IO, &action, NULL);

    /* Start child process. */
    int child_stdin[2], child_stdout[2], child_stderr[2];
    check(pipe(child_stdin), "pipe()");
    check(pipe(child_stdout), "pipe()");
    check(pipe(child_stderr), "pipe()");

    child = check(fork(), "fork()");
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
        check(dup2(child_stdin[0], STDIN_FILENO), "dup2()");
        check(dup2(child_stdout[1], STDOUT_FILENO), "dup2()");
        check(dup2(child_stderr[1], STDERR_FILENO), "dup2()");
        check(execvp(argv[2], &argv[2]), "execvp()");
    }

    child_in = child_stdin[1];
    child_out = child_stdout[0];
    child_err = child_stderr[0];
    setup_io(child_out);
    setup_io(child_err);

    atexit(LAMBDA(void, (void) { kill(child, SIGTERM); }));

    /* Set up listening socket. */
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_name);

    server = check(socket(AF_UNIX, SOCK_STREAM, 0), "socket()");
    check(bind(server, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)),
          "bind()");
    atexit(LAMBDA(void, (void) { unlink(socket_name); }));
    setup_io(server);
    check(listen(server, BACKLOG), "listen()");

    /* Set up stdin. */
    setup_io(STDIN_FILENO);

    /* Return exit status of child. */
    siginfo_t info;
    check(waitid(P_PID, child, &info, WEXITED), "waitid()");
    return info.si_status;
}

static void setup_io(int fd) {
    check(fcntl(fd, F_SETSIG, SIGRT_IO), "fcntl()");
    check(fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK), "fcntl()");
    check(fcntl(fd, F_SETOWN, getpid()), "fcntl()");
}

static void handle_io(int signum, siginfo_t *info, void *context) {
    int fd = info->si_fd;

    if (fd == STDIN_FILENO) return stdin_io();
    if (fd == server) return server_io();
    if (fd == child_out || fd == child_err) return child_io(fd);

    for (ilist *item = clients; item != NULL; item = item->tail)
        if (fd == item->head) return client_io(fd);
}

static void server_io (void) {
    for (;;) {
        int client = accept(server, NULL, 0);
        if (client == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        check(client, "accept()");
        setup_io(client);
        clients = ilist_add(clients, client);
        report("connect(%d)\n", client);
    }
}

static void client_io (int client) {
    char *line;
    ssize_t size = buffer_readline(client, &line);
    if (size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;

    if (size == 0 || size == -1) {
        clients = ilist_del(clients, client);
        close(client);
        buffer_clear(client);
        if (size == 0) report("exit(%d)\n", client);
        else report("exit(%d): %s\n", client, strerror(errno));
        return;
    }

    report("(%d) ", client);
    check(write(STDERR_FILENO, line, size), "write()");
    if (line[size-1] != '\n') fprintf(stderr, "\\\n");

    check(write(child_in, line, size), "write()");

    client_io(client);
}

static void stdin_io(void) {
    for (;;) {
        ssize_t bytes = splice(STDIN_FILENO, NULL, child_in,
                               NULL, BUFFER_SIZE, SPLICE_F_NONBLOCK);
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        check(bytes, "splice()");
    }
}

static void child_io(int fd) {
    for (;;) {
        char *rdata;
        size_t rsize;
        char *line;
        ssize_t size = buffer_readline_r(fd, &line, &rsize, &rdata);
        int again = size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);

        if (!again) check(size, "read()");
        if (size == 0) exit(EXIT_SUCCESS);

        int target = (fd == child_out) ? STDOUT_FILENO : STDERR_FILENO;
        check(write(target, rdata, rsize), "write()");

        if (again) break;

        for (ilist *i = clients; i != NULL; i = i->tail) {
            int client = i->head;
            int flags = check(fcntl(client, F_GETFL), "fcntl()");
            check(fcntl(client, F_SETFL, flags & ~O_NONBLOCK), "fcntl()");
            check(write(client, line, size), "write()");
            check(fcntl(client, F_SETFL, flags), "fcntl()");
        }
    }
}

static int check (int result, char *source) {
    if (result == -1) err(EXIT_FAILURE, source);
    return result;
}

static void report(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "%s: ", program_name);
    vfprintf(stderr, format, ap);
}
