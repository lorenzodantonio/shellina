#include "builtins.h"
#include "shell.h"

#include <stdbool.h>
#include <stdio.h>

// #include <sys/types.h>
#include <string.h>
#include <unistd.h>

int quit(struct shell *shell, int argc, char **argv) {
  (void)argc;
  (void)argv;
  shell->running = false;
  return 0;
}

int noop(struct shell *shell, int argc, char **argv) {
  (void)shell;
  (void)argc;
  (void)argv;
  return 0;
}

int cd(struct shell *shell, int argc, char **argv) {
  (void)shell;
  if (argc == 1) {
    char *home = getenv("HOME");
    if (!home) {
      fprintf(stderr, "HOME not set\n");
      return -1;
    }

    return chdir(home);
  }
  return chdir(argv[1]);
}

static struct builtin builtins[] = {
    {":", noop},
    {"cd", cd},
    {"exit", quit},
    {NULL, NULL},
};

builtin_func builtins_get(char *name) {
  for (size_t i = 0; builtins[i].func != NULL; i++) {
    if (strcmp(builtins[i].name, name) == 0) {
      return builtins[i].func;
    }
  }
  return NULL;
}
