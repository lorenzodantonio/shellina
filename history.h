#pragma once
#include "datastructure.h"
#include <stdbool.h>
#include <stdlib.h>

struct history {
  struct string **commands;
  size_t head;
  size_t capacity;
  size_t count;
  size_t cursor;
  bool active;
};

struct history *history_new(size_t capacity);
void history_push(struct history *history, struct string *command);
void history_free(struct history *history);

struct string *history_next(struct history *history);
struct string *history_prev(struct history *history);
