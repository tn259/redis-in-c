#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void hexstring(const char* in) {
    size_t len = strlen(in);
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", in[i]);
    }
    puts("\n");
}