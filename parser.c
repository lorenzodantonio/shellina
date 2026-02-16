#include "parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct parser *parser_new(char *line) {
  struct parser *p = malloc(sizeof(*p));
  p->line = line;
  p->cursor = line;
  p->state = STATE_NORMAL;
  return p;
}

void parser_free(struct parser *p) {
  //
  free(p);
}

char **tokenize(char *line, size_t *arg_count) {
  struct parser parser = {.line = line, .cursor = line, .state = STATE_NORMAL};
  char **argv = malloc(sizeof(char *) * 512);

  char *start;
  while (parser.cursor[0] != '\0') {
    if (!isspace(*parser.cursor)) {
      start = parser.cursor;
      while (*parser.cursor != '\0' && !isspace(*parser.cursor)) {
        parser.cursor++;
      }

      size_t len = parser.cursor - start;
      char *str = malloc(len + 1);
      memcpy(str, start, len);
      str[len] = '\0';

      argv[(*arg_count)++] = str;
    } else {
      start = ++parser.cursor;
    }
  }
  argv[*arg_count] = NULL;

  return argv;
}
