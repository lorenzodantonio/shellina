#pragma once

#include <stdlib.h>

struct string {
  size_t len;
  char value[];
};

struct string *string_new(char *s);
struct string *string_clone(struct string *other);
