#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "server.h"
#include "command.h"
#include "utils.h"

#define PORT "6379"

// Pending connections in queue
#define BACKLOG 10

static void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    // Spin until child has exited
    // Returns 0 when all children have finished changing state
    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
#pragma clang diagnostic pop
}

// client handler with connection fd
static void handle_client(int fd) {
    ssize_t read;
    char request_buffer[BUF_SIZE];
    char response_buffer[BUF_SIZE];
    int flags = 0;
    while ((read = recv(fd, request_buffer, sizeof request_buffer, flags)) > 0) {
        if (!handle_command(request_buffer, response_buffer)) {
            continue;
        }
        ssize_t sent = send(fd, response_buffer, sizeof response_buffer, flags);
        if (sent == -1) {
            perror("server: send");
        } 
    }
}

void serve(void) {
    // getaddrinfo() for ourselves
    // socket()
    // bind() the socket
    // listen() on the new socket
    // while {
    // accept -> newfd then send and recv in child process
    // later move onto libevent or libuv
    //}
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use server IP

    int rv;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "server: getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // bind to the first result possible
    int sockfd;
    struct addrinfo *p;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("server: setsockopt");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        // success    
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind on any addrinfo\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("server: listen");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("server: sigaction");
        exit(1);
    }

    printf("server: awaiting connections...\n");

    while (true) {
        // accept
        struct sockaddr_storage their_addr_storage;
        socklen_t addr_size = sizeof their_addr_storage;
        struct sockaddr* their_addr = (struct sockaddr*)&their_addr_storage;
        int new_fd = accept(sockfd, their_addr, &addr_size);
        if (new_fd == -1) {
            perror("server: accept");
            continue;
        }

        // print client
        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr_storage.ss_family, get_in_addr(their_addr), s, sizeof s);
        printf("server: connection from %s", s);

        if (!fork()) {
            // child
            handle_client(new_fd);
            close(sockfd);
        } else {
            // parent carries on
            close(new_fd);
        }
    }
}