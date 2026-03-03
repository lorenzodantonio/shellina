#include "datastructure.h"

#include <string.h>

struct string *string_new(char *s) {
  size_t len = strlen(s);
  struct string *str = malloc(sizeof(*str) + len + 1);
  memcpy(str->value, s, len + 1);
  str->len = len;
  return str;
}

struct string *string_clone(struct string *other) {
  size_t len = other->len;
  struct string *s = malloc(sizeof(*s) + len + 1);
  memcpy(s, other, sizeof(*s) + len + 1);
  return s;
}
