#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct RespType RespType_t;

typedef struct SimpleString {
    char *value;
} SimpleString_t;

typedef struct Error {
    char *value;
} Error_t;

typedef struct Integer {
    int64_t value;
} Integer_t;

typedef struct BulkString {
    uint8_t *value;
    int size;
} BulkString_t;

typedef struct Array {
    RespType_t *element;
    int element_count;
} Array_t;

typedef enum RespTypeEnum {
    SIMPLE_STRING,
    ERROR,
    INTEGER,
    BULKSTRING,
    ARRAY,
    UNKNOWN
} RespTypeEnum_t;

typedef struct RespType { 
    RespTypeEnum_t type;
    union {
        SimpleString_t simple_string;
        Error_t error;
        Integer_t integer;
        BulkString_t bulkstring;
        Array_t array;
    };
} RespType_t;

typedef enum DeserializeResultEnum {
    OK,
    INPUT_INVALID
} DeserializeResultEnum_t;

typedef struct DeserializeResult {
    DeserializeResultEnum_t res;
    size_t len_consumed;
} DeserializeResult_t;

char* serialize_resp(const RespType_t *resp);
DeserializeResult_t deserialize_resp(const char *resp_str, RespType_t* in);
void free_resp(RespType_t * resp);

void copy_bs(BulkString_t* dst, BulkString_t* src);
BulkString_t create_bs(char* str);
void print_bs(BulkString_t* bs);
