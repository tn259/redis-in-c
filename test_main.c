#include "resp.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void resp_test(char* in) {
    RespType_t r = deserialize_resp(in, strlen(in));
    char *out = serialize_resp(&r);
    assert(strcmp(in, out) == 0);
    free_resp(&r);
    free(out);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    resp_test((char*)"+OK\r\n");
    resp_test((char*)"+hello world\r\n");
    return 0;
}
