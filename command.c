#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "resp.h"
#include "command.h"
#include "utils.h"

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

CommandError_t parse_command(char* in, Command_t* out) {
    // Deseralize resp and check result
    RespType_t resp;
    memset(&resp, 0, sizeof(RespType_t));
    CommandError_t command_err;
    memset(&command_err, 0, sizeof(CommandError_t));
    command_err.type = COMMAND_OK;
    DeserializeResult_t res = deserialize_resp(in, &resp);
    if (res.res != OK) {
        free_resp(&resp);
        command_err.type = SYNTAX;
        command_err.message = (char*)"ERR invalid resp syntax";
        return command_err;
    }
    // Must be an array of bulk strings
    if (resp.type != ARRAY) {
        free_resp(&resp);
        command_err.type = SYNTAX;
        command_err.message = (char*)"ERR request is not a resp array";
        return command_err;
    }
    Array_t arr = resp.array;
    for (int i = 0; i < arr.element_count; ++i) {
        if (arr.element[i].type != BULKSTRING) {
            free_resp(&resp);
            command_err.type = SYNTAX;
            command_err.message = (char*)"ERR request array has an element not of type bulk string";
            return command_err;
        }
    }
    // find the first space for the command
    BulkString_t* command = &resp.array.element->bulkstring;
    if (is_command(command, (char*)PING)) {
        parse_ping(&resp, out);
    } else if (is_command(command, (char*)ECHO)) {
        if (!parse_echo(&resp, out)) {
            command_err.type = SYNTAX;
            command_err.message = (char*)"ERR bad echo request";
        }
    } else {
        command_err.type = UNKNOWN_COMMAND;
        command_err.message = (char*)"ERR unknown command";
    }
    free_resp(&resp);
    return command_err; 
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
        default:
            assert(false);
    }
}

void handle_command(char* command_req, char* command_res) {
    Command_t command;
    memset(&command, 0, sizeof(Command_t));
    CommandError_t err = parse_command(command_req, &command);
    if (err.type == COMMAND_OK) {
        generate_response(&command, command_res);
    } else {
        generate_error_response(&err, command_res);
    }
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
