#include "eval.h"
#include "builtins.h"
#include <stdio.h>
#include <unistd.h>

int eval(struct shell *shell, struct ast_node *ast) {
  switch (ast->type) {
  case AST_NODE_CMD:
    return eval_command(shell, ast);
  case AST_NODE_AND:
    if (eval(shell, ast->data.child.left) == 0) {
      return eval(shell, ast->data.child.right);
    }
    break;
  case AST_NODE_PIPE:
    return eval_pipe(shell, ast);
  case AST_NODE_ASSIGNMENT:
    param_registry_set(shell->param_registry, ast->data.assignment.label,
                       ast->data.assignment.value);
    return 0;
  }
  return -1;
}

int eval_command(struct shell *shell, struct ast_node *cmd) {
  builtin_func builtin = builtins_get(cmd->data.args[0]);

  if (builtin != NULL) {
    size_t argc = 0;
    while (cmd->data.args[argc] != NULL) {
      argc++;
    }

    int result = builtin(shell, argc, cmd->data.args);
    if (result != 0) {
      perror(cmd->data.args[0]);
    }
    return result;
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
  return 0;
}

int eval_pipe(struct shell *shell, struct ast_node *p) {
  int fd[2];
  if (pipe(fd) == -1) {
    perror("pipe");
    return -1;
  }

  pid_t p1 = fork();
  if (p1 == 0) {
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    close(fd[1]);
    exit(eval(shell, p->data.child.left));
  }

  pid_t p2 = fork();
  if (p2 == 0) {
    dup2(fd[0], STDIN_FILENO);
    close(fd[0]);
    close(fd[1]);
    exit(eval(shell, p->data.child.right));
  }
  close(fd[0]);
  close(fd[1]);

  int status;
  waitpid(p1, NULL, 0);
  waitpid(p2, &status, 0);

  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
