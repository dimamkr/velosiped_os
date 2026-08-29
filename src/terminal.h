#include "konsole.h"
#include "keyboard.h"
#include "argparse.h"

typedef void (*terminal_command_handler_cb) (argparse_command_t*);

void terminal_main_loop(void);
void terminal_read_symbol(char);
void terminal_register_command_handler(char *command, terminal_command_handler_cb handler);