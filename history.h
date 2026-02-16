#pragma once
#include <stdlib.h>

struct history {
  char **commands;
  size_t head;
  size_t capacity;
  size_t count;
};

struct history *history_new(size_t capacity);
void history_push(struct history *history, char *command);
void history_free(struct history *history);
