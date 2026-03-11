#include "history.h"
#include <stdlib.h>
#include <string.h>

struct history *history_new(size_t capacity) {
  struct history *h = malloc(sizeof(*h));
  h->capacity = capacity;
  h->commands = calloc(capacity, sizeof(struct string *));
  h->count = 0;
  h->head = 0;
  return h;
}

void history_push(struct history *h, struct string *cmd) {
  if (h->commands[h->head]) {
    free(h->commands[h->head]);
  }
  h->commands[h->head] = cmd;

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

struct string *history_prev(struct history_viewer *viewer,
                            struct history *history) {
  if (history->count <= 0) {
    return NULL;
  }

  size_t oldest = (history->count < history->capacity) ? 0 : history->head;

  if (!viewer->active) {
    viewer->active = true;
    viewer->cursor =
        (history->head - 1 + history->capacity) % history->capacity;
  } else {
    if (viewer->cursor == oldest) {
      viewer->cursor =
          (history->head - 1 + history->capacity) % history->capacity;
    } else {
      viewer->cursor =
          (viewer->cursor - 1 + history->capacity) % history->capacity;
    }
  }

  return history->commands[viewer->cursor];
}

struct string *history_next(struct history_viewer *viewer,
                            struct history *history) {
  if (history->count == 0 || !viewer->active) {
    return NULL;
  }

  size_t newest = (history->head - 1 + history->capacity) % history->capacity;
  size_t oldest = (history->count < history->capacity) ? 0 : history->head;

  if (viewer->cursor == newest) {
    viewer->cursor = oldest;
  } else {
    viewer->cursor =
        (viewer->cursor + 1 + history->capacity) % history->capacity;
  }

  return history->commands[viewer->cursor];
}

struct history_viewer *history_viewer_new(void) {
  struct history_viewer *v = malloc(sizeof(*v));

  v->active = false;
  v->cursor = 0;
  return v;
}
