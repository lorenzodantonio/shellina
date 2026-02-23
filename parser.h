#pragma once

#include <stdlib.h>

enum lexer_state {
  STATE_NORMAL,
  STATE_IN_QUOTE,
};

enum token_type {
  TOKEN_WORD,
  TOKEN_PIPE,
};

struct token {
  char *str;
  enum token_type type;
};

void token_free(struct token *token);

struct lexer {
  char *line;
  char *cursor;
  enum lexer_state state;
};

struct token_list {
  struct token **tokens;
  size_t count;
  size_t capacity;
};

struct lexer *lexer_new(char *line);
void lexer_free(struct lexer *p);
struct token_list *tokenize(char *line);

enum ast_node_type {
  AST_NODE_CMD,
  AST_NODE_PIPE,
};

struct parser {
  struct token_list *lst;
  struct token *cursor;
};

struct ast_node {
  enum ast_node_type type;
  union {
    char **args;
    struct {
      struct ast_node *left;
      struct ast_node *right;
    } child;
  } data;
};

void ast_free(struct ast_node *node);

struct ast_node *parse(struct token_list *lst, size_t start, size_t end);
