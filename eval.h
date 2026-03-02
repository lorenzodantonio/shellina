#pragma once

#include "parser.h"

struct shell;

int eval(struct shell *shell, struct ast_node *ast);
int eval_command(struct shell *shell, struct ast_node *ast);
int eval_pipe(struct shell *shell, struct ast_node *ast);
