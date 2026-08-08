#include "resp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void resp_test(char* in) {
    RespType_t r = deserialize_resp(in, strlen(in));
    char *out = serialize_resp(&r);
    assert(strcmp(in, out) == 0);
    free_resp(&r);
    free(out);
}

static void runtests(void) {
    // Simple strings
    resp_test((char*)"+OK\r\n");
    resp_test((char*)"+hello world\r\n");
    // Error strings
    resp_test((char*)"-Error message\r\n");
    // Integers
    // We omit the '+' if integer comes in with a '+'
    resp_test((char*)":0\r\n");
    resp_test((char*)":-1230\r\n");
    resp_test((char*)":4321\r\n");
    // Bulk strings
    resp_test((char*)"$5\r\nhello\r\n");
    resp_test((char*)"$-1\r\n");
    resp_test((char*)"$3\r\n\xf2\xf4\xf5\r\n");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "--test") == 0) {
            puts("Running tests\n");
            runtests();
            return 0;
        }
    }

    return 0;
}
