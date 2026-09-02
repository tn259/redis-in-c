#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

void hexstring(const char* in) {
    size_t len = strlen(in);
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", in[i]);
    }
    puts("\n");
}

int generate_rand(void) {
    // 1. Seed the random number generator using the current time
    // This should only be called ONCE at the start of your program.
    srand((unsigned int)time(NULL));

    // 2. Generate a random number within a specific range (e.g., 1 to 100)
    int min = 1;
    int max = 100;
    int random_num = (rand() % (max - min + 1)) + min;
    return random_num;
}