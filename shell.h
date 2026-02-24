#pragma once

#include "history.h"
#include "params.h"
#include <stdbool.h>
#include <stdlib.h>

struct shell {
  struct history *history;
  struct param_registry *param_registry;
  bool running;
};

struct shell *shell_new(void);
void shell_run(struct shell *shell);
void shell_free(struct shell *shell);

void shell_history_restore(struct shell *shell);
void shell_toggle_input_mode(struct shell *shell);

// void tokenize(char *line, char *args[], size_t *arg_count);
// void exec(pid_t pid, int argc, char **argv);
