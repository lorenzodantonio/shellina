#include "params.h"
#include <stdio.h>
#include <string.h>

struct param_registry *param_registry_new(size_t initial_capacity) {
  struct param_registry *registry = malloc(sizeof(*registry));

  registry->vars = malloc(sizeof(struct param) * initial_capacity);
  registry->count = 0;
  registry->capacity = initial_capacity;

  return registry;
}

struct param *param_registry_find(struct param_registry *registry,
                                  struct string *label) {
  for (size_t i = 0; i < registry->count; i++) {
    if (strcmp(registry->vars[i].label->value, label->value) == 0) {
      return &registry->vars[i];
    }
  }
  return NULL;
}

void param_registry_free(struct param_registry *registry) {
  for (size_t i = 0; i < registry->count; i++) {
    free(registry->vars[i].label);
    free(registry->vars[i].value);
  }
  free(registry->vars);
  free(registry);
}

void param_registry_set(struct param_registry *registry, struct string *label,
                        struct string *value) {
  struct param *param = param_registry_find(registry, label);
  if (param) {
    free(param->value);
    param->value = string_clone(value);
    return;
  }

  if (registry->capacity == registry->count) {
    size_t newcap = registry->capacity * 2;
    registry->vars = realloc(registry->vars, sizeof(struct param) * newcap);
    registry->capacity = newcap;
  }

  registry->vars[registry->count].value = string_clone(value);
  registry->vars[registry->count].label = string_clone(label);
  registry->count++;
}
