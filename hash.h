#pragma once

#include <stdint.h>

void derive_secret(uint64_t s);
uint64_t hash(char* str);