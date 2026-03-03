#pragma once

#include "datastructure.h"
#include <stdbool.h>
#include <stdlib.h>

struct param {
  struct string *label;
  struct string *value;
  bool exported;
};

struct param_registry {
  struct param *vars;
  size_t count;
  size_t capacity;
};

struct param_registry *param_registry_new(size_t initial_capacity);
void param_registry_free(struct param_registry *params);
void param_registry_set(struct param_registry *params, char *label,
                        char *value);

struct param *param_registry_find(struct param_registry *params, char *label);
