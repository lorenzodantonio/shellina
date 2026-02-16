#include "history.h"
#include <stdlib.h>
#include <string.h>

struct history *history_new(size_t capacity) {
  struct history *h = malloc(sizeof(*h));
  h->capacity = capacity;
  h->count = 0;
  h->head = 0;
  h->commands = calloc(capacity, sizeof(char *));
  return h;
}

void history_push(struct history *h, char *cmd) {
  if (h->commands[h->head]) {
    free(h->commands[h->head]);
  }
  h->commands[h->head] = strdup(cmd);

  if (h->count < h->capacity) {
    h->count++;
  }
  h->head = (h->head + 1) % h->capacity;
}

void history_free(struct history *h) {
  for (size_t i = 0; i < h->capacity; i++) {
    free(h->commands[i]);
  }
  free(h->commands);
  free(h);
}
