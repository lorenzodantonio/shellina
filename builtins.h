#pragma once

struct shell;

typedef int (*builtin_func)(struct shell *shell, int argc, char **argv);

struct builtin {
  char *name;
  builtin_func func;
};

builtin_func builtins_get(char *name);

int noop(struct shell *shell, int argc, char **argv);
int quit(struct shell *shell, int argc, char **argv);
int cd(struct shell *shell, int argc, char **argv);
