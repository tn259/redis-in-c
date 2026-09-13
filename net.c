#include "net.h"

#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

#ifndef UNIT_TEST

ssize_t net_send(int fd, const void* data, size_t len, int flags) {
    return send(fd, data, len, flags);
}
ssize_t net_recv(int fd, void* data, size_t len, int flags) {
    return recv(fd, data, len, flags);
}

#else

#include "utils.h"
#include <assert.h>

static char mock_recv_buffer[BUF_SIZE] = {0};
static size_t mock_recv_len = 0;
static char mock_send_buffer[BUF_SIZE] = {0};
static size_t mock_send_len = 0;

void reset_mocks(void) {
    memset(mock_recv_buffer, 0, BUF_SIZE);
    mock_recv_len = 0;
    memset(mock_send_buffer, 0, BUF_SIZE);
    mock_send_len = 0;
}

void assert_expected_sent(char* expected) {
    size_t expected_len = strlen(expected);
    assert(expected_len == mock_send_len);
    assert(strncasecmp(expected, (char*)mock_send_buffer, mock_send_len) == 0);
}

void feed_recv_data(char* data) {
    size_t len = strlen(data);
    memcpy(mock_recv_buffer, data, len);
    mock_recv_len = len; 
}

/* Populate the mock structs with data sent */
ssize_t net_send(int fd, const void* data, size_t len, int flags) {
    (void)flags;
    (void)fd;
    mock_send_len = len;
    memcpy(mock_send_buffer, data, len);
    mock_send_buffer[len] = '\0';
    return (ssize_t)len;
}

/* Populate the recv structs with the data in mocks */
ssize_t net_recv(int fd, void* data, size_t len, int flags) {
    (void)flags;
    (void)fd;
    (void)len;
    memcpy(data, mock_recv_buffer, mock_recv_len);
    char* d = (char*)data;
    d[mock_recv_len] = '\0';
    return (ssize_t)mock_recv_len;
}

#endif