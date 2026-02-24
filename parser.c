#include "parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct token *token_new(enum token_type type, char *str) {
  struct token *t = malloc(sizeof(*t));
  t->str = strdup(str);
  t->type = type;
  return t;
}

void token_free(struct token *t) {
  free(t->str);
  free(t);
}

void token_list_push(struct token_list *lst, struct token *t) {
  if (lst->count == lst->capacity) {
    size_t newcap = lst->capacity * 2;
    lst->tokens = realloc(lst->tokens, sizeof(*lst->tokens) * newcap);
    lst->capacity = newcap;
  }

  lst->tokens[lst->count++] = t;
}

struct lexer *lexer_new(char *line) {
  struct lexer *l = malloc(sizeof(*l));
  l->line = line;
  l->cursor = line;
  l->state = STATE_NORMAL;
  return l;
}

void lexer_free(struct lexer *l) {
  //
  free(l);
}

struct token *lexer_next(struct lexer *l) {
  while (isspace(*l->cursor)) {
    l->cursor++;
  }

  if (!*l->cursor) {
    return NULL;
  }

  if (*l->cursor == '|') {
    l->cursor++;
    return token_new(TOKEN_PIPE, "|");
  }

  char tstr[4096];
  size_t tlen = 0;

  while (*l->cursor) {
    if (*l->cursor == '\"') {
      l->state = (l->state == STATE_NORMAL) ? STATE_IN_QUOTE : STATE_NORMAL;
      l->cursor++;
      continue;
    }

    if (l->state == STATE_NORMAL &&
        (isspace(*l->cursor) || *l->cursor == '|')) {
      break;
    }
    tstr[tlen++] = *l->cursor++;
  }

  if (l->state == STATE_IN_QUOTE) {
    fprintf(stderr, "unclosed \"");
  }

  tstr[tlen] = '\0';

  return token_new(TOKEN_WORD, tstr);
}

struct token_list *tokenize(char *line) {
  struct lexer lexer = {.line = line, .cursor = line, .state = STATE_NORMAL};

  struct token_list *lst = malloc(sizeof(*lst));

  size_t lst_initcap = 256;
  lst->count = 0;
  lst->capacity = lst_initcap;
  lst->tokens = malloc(sizeof(*lst->tokens) * lst_initcap);

  for (struct token *t; (t = lexer_next(&lexer)) != NULL;
       token_list_push(lst, t))
    ;

  // struct token *t;
  // while ((t = parser_next(&lexer)) != NULL) {
  //   token_list_push(lst, t);
  // };

  return lst;
}

struct ast_node *ast_node_new(enum ast_node_type type) {
  struct ast_node *n = malloc(sizeof(*n));
  n->type = type;
  memset(&n->data, 0, sizeof(n->data));
  return n;
}

void ast_free(struct ast_node *node) {
  switch (node->type) {
  case AST_NODE_CMD: {
    size_t i = 0;
    while (node->data.args[i] != NULL) {
      free(node->data.args[i++]);
    }
    free(node->data.args);
    free(node);
    break;
  }
  case AST_NODE_PIPE:
    ast_free(node->data.child.left);
    ast_free(node->data.child.right);
    free(node);
    break;
  case AST_NODE_ASSIGNMENT:
    free(node->data.assignment.label);
    free(node->data.assignment.value);
    free(node);
    break;
  }
}

struct ast_node *parse_command(struct token_list *lst, size_t start,
                               size_t end) {
  struct ast_node *n = ast_node_new(AST_NODE_CMD);
  size_t argc = end - start;
  n->data.args = malloc(sizeof(char *) * (argc + 1));
  for (size_t i = 0; i < argc; i++) {
    n->data.args[i] = strdup(lst->tokens[start + i]->str);
  }
  n->data.args[argc] = NULL;
  return n;
}

struct ast_node *parse_assignment(struct token_list *lst, size_t start) {
  struct ast_node *n = ast_node_new(AST_NODE_ASSIGNMENT);
  char *str = lst->tokens[start]->str;
  char *eq = strchr(str, '=');
  *eq = '\0';

  n->data.assignment.label = strdup(str);
  n->data.assignment.value = strdup(eq + 1);

  return n;
}

struct ast_node *parse(struct token_list *lst, size_t start, size_t end) {
  for (size_t i = start; i < end; i++) {
    if (lst->tokens[i]->type == TOKEN_PIPE) {
      struct ast_node *n = ast_node_new(AST_NODE_PIPE);
      n->data.child.left = parse_command(lst, start, i);
      n->data.child.right = parse(lst, i + 1, end);
      return n;
    }
  }

  if ((end - start) == 1 && strchr(lst->tokens[start]->str, '=') != NULL) {
    return parse_assignment(lst, start);
  }

  return parse_command(lst, start, end);
}
