#include "shell.h"

int main(void) {
  struct shell *shell = shell_new();
  shell_history_restore(shell);
  shell_run(shell);
  shell_free(shell);
  return 0;
}
