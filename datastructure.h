#pragma once

#include <stdlib.h>

struct string {
  size_t len;
  char value[];
};

struct string *string_new(char *s);
