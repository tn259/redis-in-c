#pragma once

#include <stdbool.h>

#define PING "PING"
#define PONG "PONG"

#define ECHO "ECHO"

#include "resp.h"

typedef enum CommandType {
    PING_T,
    ECHO_T
} CommandType_t;

// Commands contain their RESP type constituents
typedef struct PingCommand {
    BulkString_t message;
} PingCommand_t;
typedef struct EchoCommand {
    BulkString_t message;
} EchoCommand_t;
typedef struct Command {
    CommandType_t type;
    union {
        PingCommand_t ping;
        EchoCommand_t echo;
    };
} Command_t;

typedef enum CommandErrorType {
    COMMAND_OK,
    UNKNOWN_COMMAND,
    SYNTAX
} CommandErrorType_t;

typedef struct CommandError {
    CommandErrorType_t type;
    char* message;
} CommandError_t;



// Interface assumes RESP encoded in and RESP encoded out
/*
in - resp encoded cmd string
out - parsed command
*/
CommandError_t parse_command(char* in, Command_t* out);
/*
req - Command
out - resp encoded response
*/
void generate_response(Command_t* in, char* out);

void free_command(Command_t* command);

// Top level input function
/*
command_req - resp encoded command request
command_res - resp encoded command response
*/
void handle_command(char* command_req, char* command_res);

void generate_error_response(CommandError_t* err, char* out);