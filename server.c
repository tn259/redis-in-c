#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include "server.h"
#include "command.h"
#include "utils.h"

#define PORT "6379"
#define MAX_EVENTS 32

// Pending connections in queue
#define BACKLOG 10

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
static void handle_client_once(int fd) {
    char request_buffer[BUF_SIZE];
    char response_buffer[BUF_SIZE];
    int flags = 0;
    ssize_t read = recv(fd, request_buffer, sizeof request_buffer, flags);
    if (read <= 0) {
        printf("server: client disconnected on fd %d\n", fd);
        close(fd); // automatically removes from kqueue
    } else {
        handle_command(request_buffer, response_buffer);
        ssize_t sent = send(fd, response_buffer, sizeof response_buffer, flags);
        if (sent == -1) {
            perror("server: send\n");
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
            perror("server: socket\n");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("server: setsockopt\n");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind\n");
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
        perror("server: listen\n");
        exit(1);
    }

    printf("server: awaiting connections...\n");

    int kq;
    struct kevent change_list[1];
    struct kevent event_list[MAX_EVENTS];

    if ((kq = kqueue()) == -1) {
        perror("server: kqueue creation failed\n");
        exit(1);
    }

    // Register server socket to watch for incoming read events (connections)    
    EV_SET(change_list, sockfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    if (kevent(kq, change_list, 1, NULL, 0, NULL) == -1) {
        perror("server: kevent registration failed\n");
        exit(1);
    }

    while (true) {
        int new_events = kevent(kq, NULL, 0, event_list, MAX_EVENTS, NULL);
        if (new_events < 0) {
            perror("server: kevent wait failed\n");
            exit(1);
        }

        // Process events
        for (int i = 0; i < new_events; ++i) {
            int current_fd = (int)event_list[i].ident;

            // Handle errors
            if (event_list[i].flags & EV_ERROR) {
                fprintf(stderr, "server: event error on fd %d\n", current_fd);
                close(current_fd);
                continue;
            }

            // Case 1: New incoming connection
            if (sockfd == current_fd) {
                // accept
                struct sockaddr_storage their_addr_storage;
                socklen_t addr_size = sizeof their_addr_storage;
                struct sockaddr* their_addr = (struct sockaddr*)&their_addr_storage;
                int new_fd = accept(sockfd, their_addr, &addr_size);
                if (new_fd == -1) {
                    perror("server: accept\n");
                    continue;
                }
                
                // Register new client fd for incoming read events
                EV_SET(change_list, new_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                if (kevent(kq, change_list, 1, NULL, 0, NULL) < 0) {
                    perror("server: kevent wait failed on new connection\n");
                    exit(1);
                }

                // print client
                char s[INET6_ADDRSTRLEN];
                inet_ntop(their_addr_storage.ss_family, get_in_addr(their_addr), s, sizeof s);
                printf("server: connection from %s\n", s);
            // Case 2: Data to read on connection
            } else if (event_list[i].filter == EVFILT_READ) {
                handle_client_once(current_fd);
            } else {
                // should not come in here
                fprintf(stderr, "server: unknown event on fd %d\n", current_fd);
                assert(false);
            }
        }
    }
    close(sockfd);
}