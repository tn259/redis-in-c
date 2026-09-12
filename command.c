#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "resp.h"
#include "command.h"
#include "utils.h"
#include "hashtable.h"

static bool is_command(BulkString_t* in, char* cmd) {
    return strncasecmp((char*)in->value, cmd, strlen(cmd)) == 0;
}

static void parse_ping(RespType_t* in, Command_t* out) {
    out->type = PING_T;
    PingCommand_t ping = {
        .message = (BulkString_t){
            .size = 0,
            .value = NULL
        }
    };
    if (in->array.element_count > 1) {
        // include message
        BulkString_t* bs = &in->array.element[1].bulkstring;
        copy_bs(&ping.message, bs);
    }
    out->ping = ping;
}
static bool parse_echo(RespType_t* in, Command_t* out) {
    out->type = ECHO_T;
    if (in->array.element_count != 2) {
        return false;
    }
    BulkString_t* bs = &in->array.element[1].bulkstring;
    EchoCommand_t echo = {
        .message = (BulkString_t) {
            .size = 0,
            .value = NULL
        }
    };
    copy_bs(&echo.message, bs);
    out->echo = echo;
    return true;
}
static bool parse_set(RespType_t* in, Command_t* out) {
    out->type = SET_T;
    if (in->array.element_count < 3) {
        // need at least SET <key> <value>
        return false;
    }
    BulkString_t* key = &in->array.element[1].bulkstring; 
    BulkString_t* value = &in->array.element[2].bulkstring; 
    SetCommand_t set = {
        .key = (BulkString_t) {
            .size = 0,
            .value = NULL
        },
        .value = (BulkString_t) {
            .size = 0,
            .value = NULL
        }
    };
    copy_bs(&set.key, key);
    copy_bs(&set.value, value);
    out->set = set;
    return true;
}
static bool parse_get(RespType_t* in, Command_t* out) {
    out->type = GET_T;
    if (in->array.element_count != 2) {
        // Must be GET <value>
        return false;
    }
    BulkString_t* key = &in->array.element[1].bulkstring; 
    GetCommand_t get = {
        .key = (BulkString_t){
            .size = 0,
            .value = NULL
        }
    };
    copy_bs(&get.key, key);
    out->get = get;
    return true;
}

static void deserialize_command_bs(char* resp_str, size_t resp_len, BulkString_t* bs, CommandRequestParseResult_t* result) {
    int next_crlf_len = len_to_next_crlf(resp_str+result->consumed);
    if (next_crlf_len < 0 || result->consumed+(size_t)next_crlf_len+2 > resp_len) {
        result->error_state.type = COMMAND_OK;
        result->completion_state = COMMAND_INCOMPLETE;
        result->consumed += strlen(resp_str+result->consumed);
        return; 
    }

    char *end = (char*)resp_str+result->consumed+next_crlf_len;
    bs->size = (int)strtol(resp_str+result->consumed, &end, 10);

    result->consumed += (size_t)next_crlf_len+2;
    if (bs->size == -1) {
        // null bulk string
        bs->value = NULL;
        result->error_state.type = COMMAND_OK;
        result->completion_state = COMMAND_COMPLETE;
        return;
    }

    // allocate value
    bs->value = malloc((size_t)bs->size + 1); // For extra NULL

    // skip over CRLF
    const char* value_start = resp_str+result->consumed;
    const int remainder_len = len_to_next_crlf(value_start);
    if (remainder_len < 0 || result->consumed+(size_t)(remainder_len+2) > resp_len) {
        result->error_state.type = COMMAND_OK;
        result->completion_state = COMMAND_INCOMPLETE;
        result->consumed += strlen(resp_str+result->consumed);
        free(bs->value);
        return; 
    }
    if (bs->size != remainder_len) {
        // error
        result->error_state.type = SYNTAX;
        result->completion_state = COMMAND_COMPLETE;
        result->error_state.message = (char*)"Invalid Bulk String length in array";
        free(bs->value);
        return;
    }

    memcpy(bs->value, value_start, (size_t)bs->size);
    bs->value[bs->size] = '\0';

    result->consumed += (size_t)(remainder_len+2);
}

static void deserialize_command_array(char* resp_str, size_t resp_len, Array_t* arr, CommandRequestParseResult_t* result) {
    int next_crlf_len = len_to_next_crlf(resp_str+result->consumed);
    if (next_crlf_len < 0 || (size_t)next_crlf_len+2 > resp_len) {
        result->completion_state = COMMAND_INCOMPLETE;
        result->error_state.type = COMMAND_OK;
        result->consumed += strlen(resp_str);
        return; 
    }

    char *end = (char *)resp_str+next_crlf_len;
    if (!element_count_is_valid(resp_str+result->consumed, end)) {
        result->completion_state = COMMAND_COMPLETE;
        result->error_state.type = SYNTAX;
        result->error_state.message = (char*)"Command array element count invalid";
        return;
    }
    arr->element_count = (int)strtol(resp_str+result->consumed, &end, 10);
    result->consumed += (size_t)next_crlf_len+2;

    if (arr->element_count == 0 || arr->element_count == -1) {
        // NULL or empty array
        result->completion_state = COMMAND_COMPLETE;
        result->error_state.type = COMMAND_OK;
        return;    
    }

    bool element_parse_fail = false;
    arr->element = malloc(sizeof(RespType_t) * (size_t)arr->element_count);
    int e = 0;
    for (; e < arr->element_count; ++e) {
        if (resp_str[result->consumed] != '$') {
            // element is not a BS
            result->consumed += 1;
            result->completion_state = COMMAND_COMPLETE;
            result->error_state.type = SYNTAX;
            result->error_state.message = (char*)"Non bulk string found in array";
            element_parse_fail = true;
            goto dca_end;
        }
        result->consumed += 1;
        RespType_t* resp_element = arr->element+e;
        resp_element->type = BULKSTRING;
        deserialize_command_bs(resp_str, resp_len, &resp_element->bulkstring, result);
        if (result->completion_state != COMMAND_COMPLETE || result->error_state.type != COMMAND_OK) {
            element_parse_fail = true;
            goto dca_end;
        }
    }

    dca_end:
        if (element_parse_fail) {
            // free items that were valid prior to the
            // invalid one
            for (int ee = 0; ee < e; ++ee) {
                free_resp(arr->element+ee);
            }
            free(arr->element);
        }
}

static CommandRequestParseResult_t deserialize_command(char* resp_str, size_t buflen, RespType_t* resp) {
    CommandRequestParseResult_t result = {
        .completion_state = COMMAND_COMPLETE,
        .consumed = 0,
        .error_state = (CommandError_t) {
            .type = COMMAND_OK,
            .message = NULL
        }
    };
    if (resp_str[0] != '*') {
        result.completion_state = COMMAND_COMPLETE;
        result.error_state.type = SYNTAX;
        result.error_state.message = (char*)"Resp command is not an array";
        return result;
    }
    result.consumed += 1;
    resp->type = ARRAY;
    deserialize_command_array(resp_str, buflen, &resp->array, &result);
    return result;
}

CommandRequestParseResult_t parse_command(char* in, size_t inlen, Command_t* out) {
    // Deseralize resp and check result
    RespType_t resp;
    memset(&resp, 0, sizeof(RespType_t));
    CommandRequestParseResult_t res = deserialize_command(in, inlen, &resp);
    if (res.completion_state != COMMAND_COMPLETE || res.error_state.type != COMMAND_OK) {
        return res;
    }

    // find the first space for the command
    BulkString_t* command = &resp.array.element->bulkstring;
    if (is_command(command, (char*)PING)) {
        parse_ping(&resp, out);
    } else if (is_command(command, (char*)ECHO)) {
        if (!parse_echo(&resp, out)) {
            res.error_state.type = SYNTAX;
            res.error_state.message = (char*)"ERR bad echo request";
        }
    } else if (is_command(command, (char*)SET)) {
       if (!parse_set(&resp, out)) {
            res.error_state.type = SYNTAX;
            res.error_state.message = (char*)"ERR bad set request";
       }
    } else if (is_command(command, (char*)GET)) {
        if (!parse_get(&resp, out)) {
            res.error_state.type = SYNTAX;
            res.error_state.message = (char*)"ERR bad get request";
        }
    } else {
        res.error_state.type = UNKNOWN_COMMAND;
        res.error_state.message = (char*)"ERR unknown command";
    }
    free_resp(&resp);
    return res; 
}

static void handle_ping(Command_t* req, RespType_t* response) {
    response->type = BULKSTRING;
    BulkString_t* msg = &req->ping.message;
    if (msg->size == 0) {
        // PING -> PONG
        response->bulkstring = create_bs((char*)PONG);
    } else {
        // PING [message] -> [message]
        copy_bs(&response->bulkstring, msg);
    }
}

static void handle_echo(Command_t* req, RespType_t* response) {
    response->type = BULKSTRING;
    BulkString_t* msg = &req->echo.message;
    assert(msg != NULL);
    copy_bs(&response->bulkstring, msg);
}
static void handle_set(Command_t* req, RespType_t* response) {
    if (!ht_set((char*)req->set.key.value, &req->set.value)) {
        response->type = BULKSTRING;
        // null bulk string
        response->bulkstring = (BulkString_t){
            .size = -1,
            .value = NULL
        };
    } else {
        respond_ok(response);
    }
}
static void handle_get(Command_t* req, RespType_t* response) {
    ht_get((char*)req->get.key.value, response);
}

void generate_response(Command_t* in, char* out) {
    RespType_t resp_response;
    memset(&resp_response, 0, sizeof(RespType_t));
    switch (in->type) {
        case PING_T:
            handle_ping(in, &resp_response);
            break;
        case ECHO_T:
            handle_echo(in, &resp_response);
            break;
        case SET_T:
            handle_set(in, &resp_response);
            break;
        case GET_T:
            handle_get(in, &resp_response);
            break;
        default:
            // should not get here
            assert(false);
            break;
    }
    char* serialized_resp = serialize_resp(&resp_response);
    size_t len = strlen(serialized_resp);
    memcpy(out, serialized_resp, len);
    out[len] = '\0';
    free_resp(&resp_response);
    free(serialized_resp);
    free_command(in);
}

void free_command(Command_t* command) {
    switch (command->type) {
        case PING_T:
            if (command->ping.message.value != NULL) {
                free(command->ping.message.value);
            }
            break;
        case ECHO_T:
            free(command->echo.message.value);
            break;
        case SET_T:
            free(command->set.key.value);
            free(command->set.value.value);
            break;
        case GET_T:
            free(command->get.key.value);
            break;
        default:
            assert(false);
    }
}

CommandRequestParseResult_t handle_command(char* command_req, size_t request_len, char* command_res) {
    Command_t command;
    memset(&command, 0, sizeof(Command_t));
    CommandRequestParseResult_t res = parse_command(command_req, request_len, &command);
    if (res.completion_state == COMMAND_COMPLETE) {
        if (res.error_state.type == COMMAND_OK) {
            generate_response(&command, command_res);
        } else {
            generate_error_response(&res.error_state, command_res);
        }
    }
    return res;
}

void generate_error_response(CommandError_t* error, char* out) {
    RespType_t resp;
    memset(&resp, 0, sizeof(RespType_t));
    resp.type = ERROR;
    switch (error->type) {
        case OK:
            return;
        case UNKNOWN_COMMAND:
        case SYNTAX:
            resp.error.value = error->message;
            break;
        default:
            resp.error.value = (char*)"ERR Unknown error";
    }
    char* resp_str = serialize_resp(&resp);
    // should not need to free resp or command error because it does not own the message memory
    snprintf(out, strlen(resp_str)+1, "%s", resp_str);
    free(resp_str);
}
