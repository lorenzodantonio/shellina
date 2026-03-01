#pragma once
#include <stdbool.h>
#include <stdlib.h>

struct history {
  char **commands;
  size_t head;
  size_t capacity;
  size_t count;
  size_t cursor;
  bool active;
};

struct history *history_new(size_t capacity);
void history_push(struct history *history, char *command);
void history_free(struct history *history);

char *history_next(struct history *history);
char *history_prev(struct history *history);
