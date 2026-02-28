#include "shell.h"
#include "builtins.h"

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

void handle_execpv(pid_t pid, char **argv) {
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

void run_cmd(struct shell *shell, struct ast_node *cmd) {
  builtin_func builtin = builtins_get(cmd->data.args[0]);

  if (builtin != NULL) {
    size_t argc = 0;
    while (cmd->data.args[argc] != NULL) {
      argc++;
    }

    if (builtin(shell, argc, cmd->data.args) != 0) {
      perror(cmd->data.args[0]);
    }
    return;
  }

  pid_t pid = fork();
  char **args = cmd->data.args;
  if (pid < 0) {
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    execvp(args[0], args); // on success; does not return to the caller
    // so if here; an error occurred
    perror(args[0]);
    _exit(EXIT_FAILURE);
  } else {
    wait(NULL);
  }
}

void ast_exec(struct shell *shell, struct ast_node *p);

void run_pipe(struct shell *shell, struct ast_node *p) {
  int fd[2];
  if (pipe(fd) == -1) {
    perror("pipe");
    return;
  }

  pid_t p1 = fork();
  if (p1 == 0) {
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    close(fd[1]);
    ast_exec(shell, p->data.child.left);
    exit(0);
  }

  pid_t p2 = fork();
  if (p2 == 0) {
    dup2(fd[0], STDIN_FILENO);
    close(fd[0]);
    close(fd[1]);
    ast_exec(shell, p->data.child.right);
    exit(0);
  }
  close(fd[0]);
  close(fd[1]);
  wait(NULL);
  wait(NULL);
}

void ast_exec(struct shell *shell, struct ast_node *ast) {
  switch (ast->type) {
  case AST_NODE_CMD:
    run_cmd(shell, ast);
    break;
  case AST_NODE_PIPE:
    run_pipe(shell, ast);
    break;
  case AST_NODE_ASSIGNMENT:
    param_registry_set(shell->param_registry, ast->data.assignment.label,
                       ast->data.assignment.value);
    break;
  }
}

void shell_display_prompt(void) {
  printf("%s> ", getcwd(NULL, 0));
  fflush(stdout);
}

void shell_run(struct shell *shell) {
  char line[4096];
  atexit(set_canonical_mode); // reset at exit
  while (shell->running) {
    shell_display_prompt();

    size_t pos = 0;
    char c;
    (void)shell;

    set_raw_mode();
    while (read(0, &c, 1) == 1 && c != '\n' && c != 4) {
      if (c == '\x1b') {
        char seq[2];
        if ((read(0, &seq[0], 1) == 1) && (read(0, &seq[1], 1) == 1)) {
          switch (seq[1]) {
          case 'A':
            if (shell->history->count > 0) {
              if (shell->history_viewer.active &&
                  shell->history_viewer.cursor > 1) {
                shell->history_viewer.cursor--;
              } else {
                shell->history_viewer.active = true;
                shell->history_viewer.cursor = shell->history->count;
              }
              write(1, "\r\x1b[K", 4); // clear
              shell_display_prompt();

              char *cmd =
                  shell->history->commands[shell->history_viewer.cursor - 1];

              strcpy(line, cmd);
              pos = strlen(line);
              write(1, line, pos);
            }
            break;
          case 'B':
            if (shell->history->count > 0) {
              if (shell->history_viewer.active) {
                if (shell->history_viewer.cursor < shell->history->count) {
                  shell->history_viewer.cursor++;
                } else {
                  shell->history_viewer.cursor = 1;
                }
              }

              write(1, "\r\x1b[K", 4); // clear
              shell_display_prompt();

              char *cmd =
                  shell->history->commands[shell->history_viewer.cursor - 1];

              strcpy(line, cmd);
              pos = strlen(line);
              write(1, line, pos);
            }
            break;
          }
        }

      } else if (c == 127) { // Backspace
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
    shell->history_viewer.active = false;

    if (pos > 0 && line[pos - 1] == '\n') {
      line[pos - 1] = '\0';
    }

    if (line[0] == '\0') {
      // free(line);
      continue;
    }

    shell_history_append(shell, line);

    struct token_list *token_lst = tokenize(line, shell->param_registry);

    if (token_lst->count == 0) {
      continue;
    }

    set_canonical_mode();
    struct ast_node *ast = parse(token_lst, 0, token_lst->count);
    ast_exec(shell, ast);
    ast_free(ast);

    for (size_t k = 0; k < token_lst->count; k++) {
      token_free(token_lst->tokens[k]);
    }
    free(token_lst->tokens);
    free(token_lst);
  }
  // free(line);
}
