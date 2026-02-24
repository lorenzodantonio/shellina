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

struct shell *shell_new(void) {
  struct shell *s = malloc(sizeof(*s));

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
      // free(line);
      continue;
    }

    shell_history_append(shell, line);

    struct token_list *token_lst = tokenize(line, shell->param_registry);

    if (token_lst->count == 0) {
      continue;
    }

    struct ast_node *ast = parse(token_lst, 0, token_lst->count);
    ast_exec(shell, ast);
    ast_free(ast);

    for (size_t k = 0; k < token_lst->count; k++) {
      token_free(token_lst->tokens[k]);
    }
    free(token_lst->tokens);
    free(token_lst);
  }
  free(line);
}
