#pragma once

#include <stdlib.h>

enum parser_state { STATE_NORMAL, STATE_IN_QUOTE };

struct parser {
  char *line;
  char *cursor;
  enum parser_state state;
};

struct parser *parser_new(char *line);
void parser_free(struct parser *p);
char **tokenize(char *line, size_t *arg_count);
