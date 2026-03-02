#include "shell.h"
#include "builtins.h"

#include "eval.h"
#include "parser.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

struct termios term;

struct shell *shell_new(void) {
  struct shell *s = malloc(sizeof(*s));

  s->raw_mode = false;
  s->param_registry = param_registry_new(1024);
  s->history = history_new(1024);
  s->running = true;
  return s;
}

void shell_free(struct shell *shell) {
  history_free(shell->history);
  param_registry_free(shell->param_registry);
  free(shell);
}

void set_canonical_mode(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &term); }

void set_raw_mode(void) {
  tcgetattr(STDIN_FILENO, &term);

  struct termios raw = term;
  raw.c_lflag &=
      ~(ECHO | ICANON | ISIG); // No echo, no line buffer, no signals (Ctrl+C)
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
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

  fclose(f);
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

void display_prompt(void) {
  printf("%s> ", getcwd(NULL, 0));
  fflush(stdout);
}

void shell_run(struct shell *shell) {
  char line[4096];
  atexit(set_canonical_mode); // reset at exit

  while (shell->running) {
    display_prompt();

    size_t pos = 0;
    char c;
    (void)shell;

    set_raw_mode();
    while (read(0, &c, 1) == 1 && c != '\n' && c != 4) {
      if (c == '\x1b') {
        char seq[2];
        bool is_arrow = (read(0, &seq[0], 1) == 1) &&
                        (read(0, &seq[1], 1) == 1) &&
                        (seq[1] == 'A' || seq[1] == 'B');
        if (!is_arrow) {
          continue;
        }
        char *(*history_fn_ptr)(struct history *);
        history_fn_ptr = (seq[1] == 'A') ? history_next : history_prev;
        char *cmd = history_fn_ptr(shell->history);
        write(1, "\r\x1b[K", 4);
        display_prompt();
        strcpy(line, cmd);
        pos = strlen(line);
        write(1, line, pos);
      } else if (c == 127 || c == 8) { // Backspace
        if (pos > 0) {
          pos--;
          write(1, "\b \b", 3);
        }
      } else {
        line[pos++] = c;
        write(1, &c, 1);
      }
    }

    if (c == 4) {
      shell->running = false;
      break;
    }

    line[pos] = '\0';
    write(1, "\n", 1);
    shell->history->active = false;

    if (pos > 0 && line[pos - 1] == '\n') {
      line[pos - 1] = '\0';
    }

    if (line[0] == '\0') {
      continue;
    }

    shell_history_append(shell, line);
    struct token_list *token_lst = tokenize(line, shell->param_registry);

    if (token_lst->count == 0) {
      continue;
    }

    set_canonical_mode();
    struct ast_node *ast = parse(token_lst, 0, token_lst->count);
    eval(shell, ast);
    ast_free(ast);

    for (size_t k = 0; k < token_lst->count; k++) {
      token_free(token_lst->tokens[k]);
    }
    free(token_lst->tokens);
    free(token_lst);
  }
  // free(line);
}
