#include "shell.h"
#include "builtins.h"

#include "parser.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// void tokenize(char *line, char *args[], size_t *arg_count) {
//   char *cursor = line;
//   *arg_count = 0;
//   while (cursor[0] != '\0') {
//     if (!isspace(cursor[0])) {
//       args[(*arg_count)++] = cursor;
//       while (cursor[0] != '\0' && !isspace(cursor[0])) {
//         cursor++;
//       }
//       if (*cursor != '\0') {
//         *cursor = '\0';
//         cursor++;
//       }
//     } else {
//       cursor++;
//     }
//   }
//   args[*arg_count] = NULL;
// }

struct shell *shell_new(void) {
  struct shell *s = malloc(sizeof(*s));
  s->history = history_new(1024);
  s->running = true;
  return s;
}

void shell_free(struct shell *shell) {
  history_free(shell->history);
  free(shell);
}

void shell_history_restore(struct shell *shell) {
  FILE *f = fopen(".history.log", "r");
  if (!f) {
    perror("history");
    return;
  }

  fseek(f, 0, SEEK_END);
  long pos = ftell(f);
  if (pos > 0)
    pos--;

  size_t max_lines = shell->history->capacity;
  size_t found = 0;
  while (found < max_lines && pos > 0) {
    fseek(f, --pos, SEEK_SET);
    if (fgetc(f) == '\n') {
      found++;
    }
  }

  char *line = NULL;
  size_t size;

  ssize_t nread;
  while ((nread = getline(&line, &size, f)) != -1) {
    if (nread > 0) {
      line[nread - 1] = '\0';
      history_push(shell->history, line);
    }
    free(line);
    line = NULL;
    size = 0;
    nread = 0;
  }
}

void shell_history_append(struct shell *shell, char *cmd) {
  history_push(shell->history, cmd);
  FILE *f = fopen(".history.log", "a");
  if (!f) {
    perror("history");
    return;
  }

  fprintf(f, "%s\n", cmd);
  fclose(f);
}

void exec(pid_t pid, char **argv) {
  if (pid < 0) {
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    execvp(argv[0], argv); // on success; does not return to the caller
    // so if here; an error occurred
    perror(argv[0]);
    _exit(EXIT_FAILURE);
  } else {
    wait(NULL);
  }
}

void shell_run(struct shell *shell) {
  char *line = NULL;
  size_t len = 0;

  while (shell->running) {
    printf("%s> ", getcwd(NULL, 0));
    fflush(stdout);

    ssize_t line_len = getline(&line, &len, stdin);
    if (line_len < 1) {
      shell->running = false;
    }

    if (line_len > 0 && line[line_len - 1] == '\n') {
      line[line_len - 1] = '\0';
    }

    if (line[0] == '\0') {
      continue;
    }

    shell_history_append(shell, line);

    size_t arg_count = 0;
    char **args = tokenize(line, &arg_count);

    builtin_func builtin = builtins_get(args[0]);
    (void)builtin;
    if (builtin != NULL) {
      if (builtin(shell, arg_count, args) != 0) {
        perror(args[0]);
        // continue;
      }
    } else {
      exec(fork(), args);
    }

    for (size_t i = 0; i < arg_count; i++) {
      free(args[i]);
    }
    free(args);
  }

  free(line);
}
