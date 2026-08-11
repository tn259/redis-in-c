#include "resp.h"
#include "utils.h"

#include <assert.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CRLF "\r\n"

static int len_to_next_crlf(const char* str) {
    const size_t len = strlen(str);
    for (int i = 0; i < (int)len; ++i) {
        if (strncmp(&str[i], CRLF, 2) == 0) {
            return i;
        } 
    } 
    printf("No CRLF in string %s", str);
    return -1;
}

static int deserialize_ss(const char* resp_str, SimpleString_t* ss) {
    int next_crlf = len_to_next_crlf(resp_str);
    if (next_crlf < 0) {
        return -1;
    }

    size_t innersize = (size_t)next_crlf+1; // add NULL
    char* inner = malloc(innersize); // () with null byte
    snprintf(inner, innersize, "%s", resp_str);

    ss->value = inner;
    // return string length consumed
    return next_crlf+2;
}
static int deserialize_error(const char* resp_str, Error_t* e) {
    int next_crlf = len_to_next_crlf(resp_str);
    if (next_crlf < 0) {
        return -1;
    }

    size_t innersize = (size_t)next_crlf+1;
    char* inner = malloc(innersize); // () with null byte
    snprintf(inner, innersize, "%s", resp_str);

    e->value = inner;
    return next_crlf+2;  
}
static int deserialize_int(const char* resp_str, Integer_t* i) {
    int next_crlf = len_to_next_crlf(resp_str);
    if (next_crlf < 0) {
        return -1;
    }

    char *end = (char *)resp_str+(int)next_crlf;
    i->value = strtol(resp_str, &end, 10);
    return next_crlf+2; 
}
static int deserialize_bs(const char* resp_str, BulkString_t* bs) {
    // parse out size
    int next_crlf_len = len_to_next_crlf(resp_str);
    if (next_crlf_len < 0) {
        // error
        return -1;
    }

    char *end = (char*)resp_str+next_crlf_len;
    bs->size = (int)strtol(resp_str, &end, 10);

    if (bs->size == -1) {
        // null bulk string
        bs->value = NULL;
        return next_crlf_len+2;
    }

    // allocate value
    bs->value = malloc((size_t)bs->size + 1); // For extra NULL

    // skip over CRLF
    const char* value_start = resp_str+next_crlf_len+2;
    const int remainder_len = len_to_next_crlf(value_start);
    if (remainder_len < 0) {
        // error
        free(bs->value);
        return -1;
    }

    memcpy(bs->value, value_start, (size_t)bs->size);
    bs->value[bs->size] = '\0';

    int consumed = next_crlf_len+2 + remainder_len+2;
    return consumed;
}
static int deserialize_array(const char* resp_str, Array_t* arr) {
    // parse out size
    int consumed = 0;
    int next_crlf_len = len_to_next_crlf(resp_str);
    if (next_crlf_len < 0) {
        // error
        return -1;
    }
    char *end = (char *)resp_str+next_crlf_len;
    arr->element_count = (int)strtol(resp_str, &end, 10);

    if (arr->element_count == 0 || arr->element_count == -1) {
        // NULL or empty array
        return next_crlf_len+2;
    }
    consumed += next_crlf_len+2;

    // skip over CRLF
    char* elem_start = (char*)resp_str+next_crlf_len+2;

    arr->element = malloc(sizeof(RespType_t) * (size_t)arr->element_count);
    for (int e = 0; e < arr->element_count; ++e) {
        DeserializeResult_t inner_res = deserialize_resp(elem_start, arr->element+e);
        if (inner_res.res != OK) {
            free(arr->element);
            return -1;
        }
        elem_start += inner_res.len_consumed;
        consumed += inner_res.len_consumed;
    }

    return consumed;
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
        size_t lendata = strlen(data);
        size_t lenedata = strlen(e_data);
        data = realloc(data, lendata + lenedata + 1);
        // +1 as it must include the NULL byte
        snprintf(data+lendata, lenedata+1, "%s", e_data);
        free(e_data);
    }

    return data;
}

DeserializeResult_t deserialize_resp(const char *resp_str, RespType_t* in) {
    int resp_str_consumed = 0;
    DeserializeResult_t result = {
        OK,
        0
    };
    switch (resp_str[0]) {
    case '+':
        in->type = SIMPLE_STRING;
        resp_str_consumed = deserialize_ss(resp_str+1, &in->simple_string);
        break;
    case '-':
        in->type = ERROR;
        resp_str_consumed = deserialize_error(resp_str+1, &in->error);
        break;
    case ':':
        in->type = INTEGER;
        resp_str_consumed = deserialize_int(resp_str+1, &in->integer);
        break;
    case '$':
        in->type = BULKSTRING;
        resp_str_consumed = deserialize_bs(resp_str+1, &in->bulkstring);
        break;
    case '*':
        in->type = ARRAY;
        resp_str_consumed = deserialize_array(resp_str+1, &in->array);
        break;
    default:
        printf("Unknown resp type char %c\n", resp_str[0]);
        in->type = UNKNOWN;
        break;
    }

    // In each case we've also consumed the type char
    resp_str_consumed += 1;
    if (resp_str_consumed > 0) {
        result.len_consumed = (size_t)resp_str_consumed;
    } else {
        result.res = INPUT_INVALID;
    }

    return result;
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
                RespType_t* r = arr->element+i;
                free_resp(r);
            }
            // just need to free the first one here
            free(arr->element);
        }
        default:
            break;
    }
}
