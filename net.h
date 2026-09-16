#pragma once

#include <unistd.h>

ssize_t net_send(int fd, const void* data, size_t len, int flags);
ssize_t net_recv(int fd, void* data, size_t len, int flags);

#ifdef UNIT_TEST

void reset_mocks(void);
void assert_expected_sent(char* expected);
void feed_recv_data(char* data, size_t start_idx, size_t len);

#endif