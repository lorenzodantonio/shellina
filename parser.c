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

char *parser_next(struct parser *p) {
  while (isspace(*p->cursor)) {
    p->cursor++;
  }

  if (!*p->cursor) {
    return NULL;
  }

  int tlen = 0;
  char *token = malloc(512);
  while (*p->cursor) {
    if (*p->cursor == '\"') {
      p->state = (p->state == STATE_NORMAL) ? STATE_IN_QUOTE : STATE_NORMAL;
      p->cursor++;
      continue;
    }

    if (p->state == STATE_NORMAL && isspace(*p->cursor)) {
      break;
    }

    token[tlen++] = *p->cursor++;
  }

  if (p->state == STATE_IN_QUOTE) {
    fprintf(stderr, "unclosed \"");
  }

  token[tlen] = '\0';
  return token;
}

char **tokenize(char *line, size_t *arg_count) {
  struct parser parser = {.line = line, .cursor = line, .state = STATE_NORMAL};
  char **argv = malloc(sizeof(char *) * 64);

  char *token;
  while ((token = parser_next(&parser)) != NULL) {
    argv[(*arg_count)++] = token;
  }

  return argv;
}
