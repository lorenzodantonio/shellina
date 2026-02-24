#include "builtins.h"
#include "params.h"
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

int export(struct shell *shell, int argc, char **argv) {
  if (argc == 1) {
    for (size_t i = 0; i < shell->param_registry->count; i++) {
      printf("%s: %s\n", shell->param_registry->vars[i].label,
             shell->param_registry->vars[i].value);
    }
    return 0;
  }
  if (argc == 2) {
    char *eq = strchr(argv[1], '=');
    *eq = '\0';
    param_registry_set(shell->param_registry, argv[1], eq + 1);

    return 0;
  }
  fprintf(stderr, "wrong arg count\n");
  return -1;
}

static struct builtin builtins[];

int set(struct shell *shell, int argc, char **argv) {
  (void)argc;
  (void)argv;

  for (size_t i = 0; i < shell->param_registry->count; i++) {
    printf("%s\n", shell->param_registry->vars[i].label);
  }

  size_t k = 0;
  while (builtins[k].name != NULL) {
    printf("%s\n", builtins[k].name);
    k++;
  }
  return 0;
}

static struct builtin builtins[] = {
    {":", noop},    {"cd", cd},   {"export", export},
    {"exit", quit}, {"set", set}, {NULL, NULL},
};

builtin_func builtins_get(char *name) {
  for (size_t i = 0; builtins[i].func != NULL; i++) {
    if (strcmp(builtins[i].name, name) == 0) {
      return builtins[i].func;
    }
  }
  return NULL;
}
