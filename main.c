#include "resp.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRINT_DEBUG 1


static void resp_test_ok(char* in) {
    RespType_t r = {
        UNKNOWN,
        {0}
    };
    DeserializeResult_t d_result = deserialize_resp(in, &r);
    assert(d_result.res == OK);
    char *out = serialize_resp(&r);
    if (PRINT_DEBUG) {
        puts("-------");
        printf("%s", in);
        hexstring(in);
        printf("%s", out);
        hexstring(out);
    }
    assert(strcmp(in, out) == 0);
    free(out);
    free_resp(&r);
}
static void resp_test_invalid(char* in) {
    RespType_t r = {
        UNKNOWN,
        {0}
    };
    DeserializeResult_t d_result = deserialize_resp(in, &r);
    assert(d_result.res == INPUT_INVALID);
}

static void runtests(void) {
    // Simple strings
    resp_test_ok((char*)"+OK\r\n");
    resp_test_ok((char*)"+hello world\r\n");
    // Error strings
    resp_test_ok((char*)"-Error message\r\n");
    // Integers
    // We omit the '+' if integer comes in with a '+'
    resp_test_ok((char*)":0\r\n");
    resp_test_ok((char*)":-1230\r\n");
    resp_test_ok((char*)":4321\r\n");
    // Bulk strings
    resp_test_ok((char*)"$5\r\nhello\r\n");
    resp_test_ok((char*)"$-1\r\n");
    resp_test_ok((char*)"$3\r\n\xf2\xf4\xf5\r\n");
    resp_test_ok((char*)"$0\r\n\r\n");
    // Arrays
    resp_test_ok((char*)"*1\r\n$4\r\nping\r\n"); // 1 value
    resp_test_ok((char*)"*0\r\n"); // empty
    resp_test_ok((char*)"*-1\r\n"); // null array
    resp_test_ok((char*)"*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n"); // 2 values
    resp_test_ok((char*)"*3\r\n:1\r\n:2\r\n:3\r\n");
    // Array with null BS inside
    resp_test_ok((char*)"*3\r\n$5\r\nhello\r\n$-1\r\n$5\r\nworld\r\n");
    // Nested array and mixed elements
    resp_test_ok((char*)"*2\r\n*3\r\n:1\r\n:2\r\n:3\r\n*3\r\n+Hello\r\n$3\r\nSTU\r\n-World\r\n");

    // Simple strings and errors
    resp_test_invalid((char*)"+OK\r");
    resp_test_invalid((char*)"+OK\n");
    resp_test_invalid((char*)"-Err\r");
    resp_test_invalid((char*)"-Err\n");
    // Bulk Strings
    resp_test_invalid((char*)"$4\rqwer\r\n");
    resp_test_invalid((char*)"$4\r\nqwer\n");
    resp_test_invalid((char*)"$3\r\nqwer\r\n");
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
