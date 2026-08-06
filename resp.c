#include "resp.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CRLF "\r\n"

static int len_to_next_crlf(const char* str) {
    const size_t len = strlen(str);
    for (int i = 0; i < (int)len; i += 2) {
        if (strncmp(&str[i], CRLF, 2) == 0) {
            return i;
        } 
    } 
    printf("No CRLF in string %s", str);
    return -1;
}

static SimpleString_t deserialize_ss(const char* resp_str, size_t len) {
    assert(len >= 2); // account for +()\r\n

    size_t innersize = len-2+1; // remove \r\n add NULL
    char* inner = malloc(innersize); // () with null byte
    snprintf(inner, innersize, "%s", resp_str);

    SimpleString_t ss = {
        .value = inner
    };
    return ss;
}
static Error_t deserialize_error(const char* resp_str, size_t len) {
    assert(len >= 2); // account for -()\r\n

    size_t innersize = len-1;
    char* inner = malloc(innersize); // () with null byte
    snprintf(inner, innersize, "%s", resp_str);

    Error_t e = {
        .value = inner
    };
    return e;  
}
static Integer_t deserialize_int(const char* resp_str, size_t len) {
    assert(len >= 2); // account for ()\r\n

    char *end = (char *)resp_str+(int)len-2;
    Integer_t i = {
        .value = strtol(resp_str, &end, 10)
    };
    return i; 
}
static BulkString_t deserialize_bs(const char* resp_str, size_t len) {
    (void)len;
    BulkString_t bs;

    // parse out size
    int next_crlf_len = len_to_next_crlf(resp_str);
    char *end = (char*)resp_str+next_crlf_len;
    bs.size = (int)strtol(resp_str, &end, 10);

    if (bs.size == -1) {
        // null bulk string
        bs.value = NULL;
        return bs;
    }

    // skip over CRLF
    const char* value_start = resp_str+next_crlf_len+2;
    const size_t remainder_len = strlen(value_start);
    assert(remainder_len >= 2); // has CRLF at end
    memcpy(bs.value, value_start, remainder_len);

    return bs;
}
static Array_t deserialize_array(const char* resp_str, size_t len) {
    (void)len;
    Array_t arr;
    
    // parse out size
    int next_crlf_len = len_to_next_crlf(resp_str);
    char *end = (char *)resp_str+next_crlf_len;
    arr.element_count = (int)strtol(resp_str, &end, 10);

    // skip over CRLF
    const char* elems_start = resp_str+next_crlf_len+2;
    const size_t remainder_len = strlen(elems_start);
    assert(remainder_len >= 2); // has CRLF at end

    arr.element = malloc(sizeof(RespType_t) * (size_t)arr.element_count);
    int i = 0;
    for (int e = 0; e < arr.element_count; ++e) {
        next_crlf_len = len_to_next_crlf(elems_start+i);
        RespType_t rt = deserialize_resp(elems_start+i, (size_t)(next_crlf_len+2-i));
        *(arr.element+e) = rt;
    }

    return arr;
}

static char* serialize_ss(const SimpleString_t* ss) {
    // 1 for '+', 2 for CRLF, 1 for NULL
    size_t size = 1 + strlen(ss->value) + 2 + 1;
    char* data = malloc(size);
    snprintf(data, size, "+%s%s", ss->value, CRLF);
    return data;
}
static char* serialize_error(const Error_t* err) {    
    // 1 for '-', 2 for CRLF, 1 for NULL
    size_t size = 1 + strlen(err->value) + 2 + 1;
    char* data = malloc(size);
    snprintf(data, size, "-%s%s", err->value, CRLF);
    return data;
}
static char* serialize_int(const Integer_t* integer) {
    // 1 for ':', 2 for CRLF, 1 for NULL
    char nbuffer[21]; // int64_t in base 10 has max 19 chars + 2 for '-' and NULL
    snprintf(nbuffer, sizeof(nbuffer), "%lld", integer->value);
    size_t datasize = 1 + strlen(nbuffer) + 2 + 1;
    char *data = malloc(datasize);
    snprintf(data, datasize, ":%s%s", nbuffer, CRLF);
    return data;
}

#define NULL_BS "$-1\r\n"

static char* serialize_bs(const BulkString_t* bs) {
    char *data;

    // NULL bulk string
    if (bs->size == -1) {
        data = malloc(sizeof(NULL_BS)+1);
        strcpy(data, NULL_BS);
        return data;
    }

    char size_str[21];
    snprintf(size_str, sizeof(size_str), "%d", bs->size);
    // '$' + int_str + CRLF + data + CRLF + NULL
    data = malloc(1 + strlen(size_str) + 2 + (size_t)bs->size + 2 + 1);
    size_t datasize = 1 + strlen(size_str) + 2 + (size_t)bs->size + 2 + 1;
    snprintf(data, datasize, "$%s%s%s%s", size_str, CRLF, (char*)bs->value, CRLF);
    return data;
}
static char* serialize_array(const Array_t* array) {
    char * data = NULL;

    char element_count_size[21];
    snprintf(element_count_size, sizeof(element_count_size), "%d", array->element_count);
    // '*' + element_count_size + CRLF + ....
    // start with just the prefix
    size_t datasize = 1 + strlen(element_count_size) + 2 + 1;
    data = malloc(datasize);
    snprintf(data, datasize, "*%s%s", element_count_size, CRLF);
    for (int e = 0; e < array->element_count; ++e) {
        char *e_data = serialize_resp(&array->element[e]);
        data = realloc(data, strlen(data) + strlen(e_data) + 1);
        strcpy(data+strlen(data), e_data);
    }

    return data;
}

RespType_t deserialize_resp(const char* resp_str, size_t len) {
    RespType_t resp;
    switch (resp_str[0]) {
    case '+':
        resp.simple_string = deserialize_ss(resp_str+1, len-1);
        resp.type = SIMPLE_STRING;
        break;
    case '-':
        resp.error = deserialize_error(resp_str+1, len-1);
        resp.type = ERROR;
        break;
    case ':':
        resp.integer = deserialize_int(resp_str+1, len-1);
        resp.type = INTEGER;
        break;
    case '$':
        resp.bulkstring = deserialize_bs(resp_str+1, len-1);
        resp.type = BULKSTRING;
        break;
    case '*':
        resp.array = deserialize_array(resp_str+1, len-1);
        resp.type = ARRAY;
        break;
    default:
        printf("Unknown resp type char %c", resp_str[0]);
        resp.type = UNKNOWN;
        break;
    }
    return resp;
}

char* serialize_resp(const RespType_t *resp) {
    if (resp == NULL) return NULL;
    char* str = NULL;
    switch (resp->type) {
        case SIMPLE_STRING:
            str = serialize_ss(&resp->simple_string);
            break;
        case ERROR:
            str = serialize_error(&resp->error);
            break;
        case INTEGER:
            str = serialize_int(&resp->integer);
            break;
        case BULKSTRING:
            str = serialize_bs(&resp->bulkstring);
            break;
        case ARRAY:
            str = serialize_array(&resp->array);
            break;
        default:
            printf("Unknown resp type %d", resp->type);
    }
    return str;
}

void free_resp(RespType_t *resp) {
    switch (resp->type) {
        case SIMPLE_STRING:
            free(resp->simple_string.value);
            break;
        case ERROR:
            free(resp->error.value);
            break;
        case INTEGER:
        case UNKNOWN:
            break;
        case BULKSTRING:
            free(resp->bulkstring.value);
            break;
        case ARRAY:
        {
            Array_t *arr = &resp->array;
            for (int i = 0; i < arr->element_count; ++i) {
                free(arr->element+i);
            }
        }
        default:
            break;
    }
}
