#ifndef ARGPARSE
#define ARGPARSE

#include "types.h"
#include "dynamic_array.h"

#define ARGPARSE_STATE_CMDNAME 0x00
#define ARGPARSE_STATE_ARGNAME 0x01
#define ARGPARSE_STATE_ARGVAL 0x02
#define ARGPARSE_STATE_INPUT 0x03

typedef struct {
    char *name;
    char *value;
} argparse_argument_t;

typedef struct {
    char *command_name;
    dynamic_array_t *arguments;
} argparse_command_t;

void argparse_parse_command(char *buffer, argparse_command_t *result);
void argparse_free_command(argparse_command_t *command);

#endif