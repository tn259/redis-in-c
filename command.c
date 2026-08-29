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

bool parse_command(char* in, Command_t* out) {
    // Deseralize resp and check result
    RespType_t resp;
    memset(&resp, 0, sizeof(resp));
    DeserializeResult_t res = deserialize_resp(in, &resp);
    if (res.res != OK) {
        free_resp(&resp);
        return false;
    }
    // Must be an array of bulk strings
    if (resp.type != ARRAY) {
        fprintf(stderr, "parse_command: command request must be an array");
        free_resp(&resp);
        return false;
    }
    Array_t arr = resp.array;
    for (int i = 0; i < arr.element_count; ++i) {
        if (arr.element[i].type != BULKSTRING) {
            fprintf(stderr, "parse_command: command request array must comprise of bulk strings");
            free_resp(&resp);
            return false;
        }
    }
    // find the first space for the command
    BulkString_t* command = &resp.array.element->bulkstring;
    bool parse_res = true;
    if (is_command(command, (char*)PING)) {
        parse_ping(&resp, out);
    } else if (is_command(command, (char*)ECHO)) {
        parse_res = parse_echo(&resp, out);
    } else {
        parse_res = false;
    }
    free_resp(&resp);
    return parse_res; 
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

bool generate_response(Command_t* in, char* out) {
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
            fprintf(stderr, "handle_command: unknown command");
            return false;
    }
    char* serialized_resp = serialize_resp(&resp_response);
    size_t len = strlen(serialized_resp);
    memcpy(out, serialized_resp, len);
    out[len] = '\0';
    free_resp(&resp_response);
    free(serialized_resp);
    free_command(in);
    return true;
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
            fprintf(stderr, "free_command: unknown command %d", command->type);
    }
}

bool handle_command(char* command_req, char* command_res) {
    Command_t command;
    memset(&command, 0, sizeof(Command_t));
    if (!parse_command(command_req, &command)) {
        fprintf(stderr, "server: handle_client: %s", command_req);
        return false;
    }
    if (!generate_response(&command, command_res)) {
        fprintf(stderr, "server: handle_command: %s", command_res);
        return false;
    }
    return true;
}

