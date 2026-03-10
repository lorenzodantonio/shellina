#include "parser.h"
#include "params.h" // TODO
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct token *token_new(enum token_type type, char *str) {
  struct token *t = malloc(sizeof(*t));
  t->str = string_new(str);
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

struct token *lexer_next(struct lexer *l, struct param_registry *params) {
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
  if (*l->cursor == '&' && *(l->cursor + 1) == '&') {
    l->cursor += 2;
    return token_new(TOKEN_AND, "&&");
  }

  char tstr[4096];
  size_t tlen = 0;

  while (*l->cursor) {
    if (*l->cursor == '\"') {
      l->state = (l->state == STATE_NORMAL) ? STATE_IN_QUOTE : STATE_NORMAL;
      l->cursor++;
      continue;
    }

    if (*l->cursor == '$' && isalnum(*(l->cursor + 1))) {
      char *start = ++l->cursor; // skip $
      while (isalnum(*l->cursor) || *l->cursor == '_') {
        l->cursor++;
      }
      size_t param_len = l->cursor - start;
      if (param_len < 1) {
        l->cursor++;
        continue;
      }
      char *param_name = malloc(param_len + 1);
      memcpy(param_name, start, param_len);
      param_name[param_len] = '\0';

      struct param *param = param_registry_find(params, string_new(param_name));
      if (param) {
        tlen += snprintf(tstr + tlen, sizeof(tstr) - tlen, "%s",
                         param->value->value);
      }

      free(param_name);
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

struct token_list *tokenize(struct string *line,
                            struct param_registry *params) {
  struct lexer lexer = {
      .line = line->value, .cursor = line->value, .state = STATE_NORMAL};

  struct token_list *lst = malloc(sizeof(*lst));

  size_t lst_initcap = 256;
  lst->count = 0;
  lst->capacity = lst_initcap;
  lst->tokens = malloc(sizeof(*lst->tokens) * lst_initcap);

  for (struct token *t; (t = lexer_next(&lexer, params)) != NULL;
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
  case AST_NODE_AND:
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
    size_t len = lst->tokens[start + i]->str->len;
    n->data.args[i] = malloc(len + 1);
    memcpy(n->data.args[i], lst->tokens[start + i]->str->value, len + 1);
  }
  n->data.args[argc] = NULL;
  return n;
}

struct ast_node *parse_assignment(struct token_list *lst, size_t start) {
  struct ast_node *n = ast_node_new(AST_NODE_ASSIGNMENT);

  struct string *str = lst->tokens[start]->str;
  char *eq = strchr(str->value, '=');
  *eq = '\0';

  n->data.assignment.label = string_new(str->value);
  n->data.assignment.value = string_new(eq + 1);

  return n;
}

struct ast_node *parse(struct token_list *lst, size_t start, size_t end) {
  if (start >= end) {
    return NULL;
  }

  for (size_t i = start; i < end; i++) {
    if (lst->tokens[i]->type == TOKEN_PIPE) {
      struct ast_node *n = ast_node_new(AST_NODE_PIPE);
      struct ast_node *left = parse(lst, start, i);
      if (!left) {
        return NULL;
      }

      struct ast_node *right = parse(lst, i + 1, end);
      if (!right) {
        free(left);
        return NULL;
      }
      n->data.child.left = left;
      n->data.child.right = right;
      return n;
    }
    if (lst->tokens[i]->type == TOKEN_AND) {
      struct ast_node *n = ast_node_new(AST_NODE_AND);
      struct ast_node *left = parse(lst, start, i);
      if (!left) {
        return NULL;
      }

      struct ast_node *right = parse(lst, i + 1, end);
      if (!right) {
        free(left);
        return NULL;
      }
      n->data.child.left = left;
      n->data.child.right = right;
      return n;
    }
  }

  char *eqpos = strchr(lst->tokens[start]->str->value, '=');

  if (eqpos != NULL && eqpos > lst->tokens[start]->str->value &&
      !isdigit(*lst->tokens[start]->str->value) && isalnum(*(eqpos - 1)) &&
      isalnum(*(eqpos + 1))) {
    return parse_assignment(lst, start);
  }

  return parse_command(lst, start, end);
}
