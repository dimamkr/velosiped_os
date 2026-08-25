#include "argparse.h"


void argparse_parse_command(char *buffer, argparse_command_t *result)
{
    uint8_t state = ARGPARSE_STATE_CMDNAME;
    bool_t quotes = false;
    uint32_t ind = 0;

    result->arguments = dynamic_array_create(sizeof(argparse_argument_t));
    argparse_argument_t arg = {0};

    result->command_name = malloc(256);

    for (uint32_t ind = 0;;buffer++)
    {
        if (ind == 0 && !quotes && *buffer == ' ')
            continue;
        if (*buffer == '\'')
        {
            quotes = !quotes;
            continue;
        }

        if (state == ARGPARSE_STATE_CMDNAME)
        {
            if (quotes == false && *buffer == ' ' || *buffer == '\0')
            {
                state = ARGPARSE_STATE_ARGNAME;
                result->command_name[ind++] = '\0';
                realloc(result->command_name, ind);
                ind = 0;
            }
            else
                result->command_name[ind++] = *buffer;
        }
        else if (state == ARGPARSE_STATE_ARGNAME)
        {
            if (*buffer == '/')
                continue;
            if (ind == 0)
                arg.name = malloc(256);
            if (quotes == false && *buffer == ' ' || *buffer == '\0')
            {
                state = ARGPARSE_STATE_ARGVAL;
                arg.name[ind++] = '\0';
                realloc(arg.name, ind);
                ind = 0;
            }
            else
                arg.name[ind++] = *buffer;
        }
        else if (state == ARGPARSE_STATE_ARGVAL)
        {
            if (ind == 0)
                arg.value = malloc(256);
            if (quotes == false && (*buffer == ' ' || *buffer == '/') || *buffer == '\0')
            {
                state = ARGPARSE_STATE_ARGNAME;
                arg.value[ind++] = '\0';
                realloc(arg.value, ind);
                ind = 0;

                dynamic_array_push_back(result->arguments, &arg);
            }
            else
                arg.value[ind++] = *buffer;
        }

        if (*buffer == '\0')
        {
            if (state == ARGPARSE_STATE_ARGVAL)
            {
                arg.value = malloc(1);
                arg.value[0] = '\0';
                dynamic_array_push_back(result->arguments, &arg);
            }

            break;
        }
    }
}

void argparse_free_command(argparse_command_t *command)
{
    free(command->command_name);

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        free(arg->name);
        free(arg->value);
    }

    dynamic_array_destroy(command->arguments);
}