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

  char line[4096];
  while (fgets(line, sizeof(line), f)) {
    size_t nread = strlen(line);
    if (nread <= 0) {
      return;
    }
    if (line[nread - 1] == '\n') {
      line[nread - 1] = '\0';
      nread--;
    }
    history_push(shell->history, string_new(line));
  }

  fclose(f);
}

void clear_line(void) { write(1, "\r\x1b[K", 4); }
void clear_char(void) { write(1, "\b \b", 3); }
void shift_cursor_left(void) { write(1, "\x1b[D", 3); }

void shell_history_append(struct shell *shell, struct string *cmd) {
  history_push(shell->history, cmd);
  FILE *f = fopen(".history.log", "a");
  if (!f) {
    perror("history");
    return;
  }

  fprintf(f, "%s\n", cmd->value);
  fclose(f);
}

void display_prompt(void) {
  printf("%s> ", getcwd(NULL, 0));
  fflush(stdout);
}

void read_escape(struct shell *shell, char *buffer, size_t *pos, size_t *len) {
  char c = '\x1b';
  // *pos = 0;
  // *len = 0;
  char seq[8];
  size_t i = 0;
  seq[i++] = c;
  if (read(0, &seq[i], 1) == 1 && seq[i] == '[') {
    i++;
    while ((read(0, &seq[i], 1) == 1) && (!isalnum(seq[i]) && seq[i] != '~')) {
      // TODO handle overflow
      i++;
    }
    switch (seq[i]) {
    case 'A':
    case 'B': {
      struct string *(*history_fn_ptr)(struct history *);
      history_fn_ptr = (seq[i] == 'A') ? history_next : history_prev;
      struct string *cmd = history_fn_ptr(shell->history);
      write(1, "\r\x1b[K", 4);
      display_prompt();
      strcpy(buffer, cmd->value);
      write(1, buffer, cmd->len);
      *pos = cmd->len;
      *len = cmd->len;
      break;
    }
    case 'C':
      if (*pos < *len) {
        (*pos)++;
        write(1, seq, i + 1);
      }
      break;

    case 'D':
      if (*pos > 0) {
        (*pos)--;
        write(1, seq, i + 1);
      }
      break;
    default:
      fprintf(stderr, "unrecognized escaped sequence\n");
    }
  }
}

void buffer_middle_delete(char *buffer, size_t *pos, size_t *len) {
  memmove(&buffer[(*pos) - 1], &buffer[*pos], *len - *pos);
  (*pos)--;
  (*len)--;
  clear_line();
  display_prompt();
  write(1, buffer, *len);
  for (size_t p = *pos; p++ < *len; shift_cursor_left())
    ;
}

void buffer_middle_insert(char *buffer, char c, size_t *pos, size_t *len) {
  memmove(&buffer[(*pos) + 1], &buffer[*pos], *len - *pos);
  buffer[*pos] = c;
  (*pos)++;
  (*len)++;
  clear_line();
  display_prompt();
  write(1, buffer, *len);
  for (size_t p = *pos; p++ < *len; shift_cursor_left())
    ;
}

void shell_run(struct shell *shell) {
  char buffer[4096];
  atexit(set_canonical_mode); // reset at exit

  while (shell->running) {
    display_prompt();
    size_t len = 0;
    size_t pos = 0;
    char c;

    set_raw_mode();
    while (read(0, &c, 1) == 1 && c != '\n' && c != 4) {
      if (c == '\x1b') {
        read_escape(shell, &buffer[0], &pos, &len);
        continue;
      }

      if (c == 127 || c == 8) { // Backspace
        if (pos == 0) {
          continue;
        }
        if (pos < len) {
          buffer_middle_delete(&buffer[0], &pos, &len);
          continue;
        }

        write(1, "\b \b", 3);
        pos--;
        len--;
        continue;
      }

      if (pos < len) {
        buffer_middle_insert(&buffer[0], c, &pos, &len);
        continue;
      }
      buffer[pos++] = c;
      len++;
      write(1, &c, 1);
    }

    if (c == 4) {
      shell->running = false;
      break;
    }

    buffer[len] = '\0';
    write(1, "\n", 1);

    shell->history->active = false;

    if (pos > 0 && buffer[pos - 1] == '\n') {
      buffer[pos - 1] = '\0';
    }

    if (buffer[0] == '\0') {
      continue;
    }

    struct string *line = string_new(buffer);

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
}
