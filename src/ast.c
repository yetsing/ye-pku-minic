#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

void ast_number_dump(AstNumber *node, int indent) {
  printf("%d", node->number);
}

AstNumber *new_ast_number() {
  AstNumber *node = malloc(sizeof(AstNumber));
  node->base.type = AST_NUMBER;
  node->base.dump = (DumpFunc)ast_number_dump;
  node->number = 0;
  return node;
}

void ast_unary_exp_dump(AstUnaryExp *node, int indent) {
  printf("UnaryExp: {\n");
  printf("%*s  op: %c,\n", indent, " ", node->op);
  printf("%*s  operand: ", indent, " ");
  node->operand->dump((AstBase *)node->operand, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstUnaryExp *new_ast_unary_exp() {
  AstUnaryExp *node = malloc(sizeof(AstUnaryExp));
  node->base.type = AST_UNARY_EXP;
  node->base.dump = (DumpFunc)ast_unary_exp_dump;
  node->op = 0;
  node->operand = NULL;
  return node;
}

void ast_stmt_dump(AstStmt *node, int indent) {
  printf("Stmt: {\n");
  printf("%*s  exp: ", indent, " ");
  node->exp->dump((AstBase *)node->exp, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstStmt *new_ast_stmt() {
  AstStmt *node = malloc(sizeof(AstStmt));
  node->base.type = AST_STMT;
  node->base.dump = (DumpFunc)ast_stmt_dump;
  node->exp = NULL;
  return node;
}

void ast_func_type_dump(AstFuncType *node, int indent) {
  printf("%s", node->name);
}

AstFuncType *new_ast_func_type() {
  AstFuncType *node = malloc(sizeof(AstFuncType));
  node->base.type = AST_FUNC_TYPE;
  node->base.dump = (DumpFunc)ast_func_type_dump;
  node->name = NULL;
  return node;
}

void ast_identifier_dump(AstIdentifier *node, int indent) {
  printf("%s", node->name);
}

AstIdentifier *new_ast_identifier() {
  AstIdentifier *node = malloc(sizeof(AstIdentifier));
  node->base.type = AST_IDENTIFIER;
  node->base.dump = (DumpFunc)ast_identifier_dump;
  node->name = NULL;
  return node;
}

void ast_block_dump(AstBlock *node, int indent) {
  printf("Block: {\n");
  printf("%*s  stmt: ", indent, " ");
  node->stmt->base.dump((AstBase *)node->stmt, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstBlock *new_ast_block() {
  AstBlock *node = malloc(sizeof(AstBlock));
  node->base.type = AST_BLOCK;
  node->base.dump = (DumpFunc)ast_block_dump;
  node->stmt = NULL;
  return node;
}

void ast_func_def_dump(AstFuncDef *node, int indent) {
  printf("FuncDef: {\n");
  printf("%*s  func_type: ", indent, " ");
  node->func_type->base.dump((AstBase *)node->func_type, indent + 2);
  printf(",\n");
  printf("%*s  ident: ", indent, " ");
  node->ident->base.dump((AstBase *)node->ident, indent + 2);
  printf(",\n");
  printf("%*s  block: ", indent, " ");
  node->block->base.dump((AstBase *)node->block, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstFuncDef *new_ast_func_def() {
  AstFuncDef *node = malloc(sizeof(AstFuncDef));
  node->base.type = AST_FUNC_DEF;
  node->base.dump = (DumpFunc)ast_func_def_dump;
  node->func_type = NULL;
  node->ident = NULL;
  node->block = NULL;
  return node;
}

void ast_comp_unit_dump(AstCompUnit *node, int indent) {
  printf("CompUnit: {\n");
  printf("  func_def: ");
  node->func_def->base.dump((AstBase *)node->func_def, indent + 2);
  printf(",\n");
  printf("}\n");
}

AstCompUnit *new_ast_comp_unit() {
  AstCompUnit *node = malloc(sizeof(AstCompUnit));
  node->base.type = AST_COMP_UNIT;
  node->base.dump = (DumpFunc)ast_comp_unit_dump;
  node->func_def = NULL;
  return node;
}