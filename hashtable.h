#pragma once

#include <stdbool.h>

#define RESIZE true

typedef struct BulkString BulkString_t;
typedef struct RespType RespType_t;

void ht_init(void);
void ht_free(void);
void ht_print(void);

/**
 * returns true if key was set
 */
bool ht_set(char* key, BulkString_t* in_value);
/**
 * BulkString or Null
 */
void ht_get(char* key, RespType_t* out_value);