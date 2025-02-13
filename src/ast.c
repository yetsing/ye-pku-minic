#include "ast.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

const char *ast_type_to_string(AstType type) {
  switch (type) {
  case AST_NUMBER:
    return "number";
  case AST_UNARY_EXP:
    return "unary_exp";
  case AST_BINARY_EXP:
    return "binary_exp";
  case AST_EXP_STMT:
    return "exp_stmt";
  case AST_RETURN_STMT:
    return "return_stmt";
  case AST_CONST_DEF:
    return "const_def";
  case AST_CONST_DECL:
    return "const_decl";
  case AST_IDENTIFIER:
    return "identifier";
  case AST_BLOCK:
    return "block";
  case AST_FUNC_DEF:
    return "func_def";
  case AST_COMP_UNIT:
    return "comp_unit";
  }
  fatalf("Invalid AstType: %d\n", type);
  return "";
}

const char *btype_to_string(BType type) {
  switch (type) {
  case BType_UNKNOWN:
    return "unknown";
  case BType_INT:
    return "int";
  }
  fatalf("Invalid BType: %d\n", type);
  return "";
}

const char *binary_op_type_to_string(BinaryOpType type) {
  switch (type) {
  case BinaryOpType_ADD:
    return "+";
  case BinaryOpType_SUB:
    return "-";
  case BinaryOpType_MUL:
    return "*";
  case BinaryOpType_DIV:
    return "/";
  case BinaryOpType_MOD:
    return "%";
  case BinaryOpType_EQ:
    return "==";
  case BinaryOpType_NE:
    return "!=";
  case BinaryOpType_LT:
    return "<";
  case BinaryOpType_LE:
    return "<=";
  case BinaryOpType_GT:
    return ">";
  case BinaryOpType_GE:
    return ">=";
  case BinaryOpType_AND:
    return "&&";
  case BinaryOpType_OR:
    return "||";
  }
  fatalf("Invalid BinaryOpType: %d\n", type);
  return "";
}

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

void ast_binary_exp_dump(AstBinaryExp *node, int indent) {
  printf("BinaryExp: {\n");
  printf("%*s  op: %s,\n", indent, " ", binary_op_type_to_string(node->op));
  printf("%*s  lhs: ", indent, " ");
  node->lhs->dump((AstBase *)node->lhs, indent + 2);
  printf(",\n");
  printf("%*s  rhs: ", indent, " ");
  node->rhs->dump((AstBase *)node->rhs, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstBinaryExp *new_ast_binary_exp() {
  AstBinaryExp *node = malloc(sizeof(AstBinaryExp));
  node->base.type = AST_BINARY_EXP;
  node->base.dump = (DumpFunc)ast_binary_exp_dump;
  node->op = 0;
  node->lhs = NULL;
  node->rhs = NULL;
  return node;
}

void ast_exp_stmt_dump(AstExpStmt *node, int indent) {
  printf("ExpStmt: {\n");
  printf("%*s  exp: ", indent, " ");
  node->exp->dump((AstBase *)node->exp, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstExpStmt *new_ast_exp_stmt() {
  AstExpStmt *node = malloc(sizeof(AstExpStmt));
  node->base.type = AST_EXP_STMT;
  node->base.dump = (DumpFunc)ast_exp_stmt_dump;
  node->exp = NULL;
  return node;
}

void ast_return_stmt_dump(AstReturnStmt *node, int indent) {
  printf("%*sReturnStmt: {\n", indent, " ");
  printf("%*s  exp: ", indent, " ");
  node->exp->dump((AstBase *)node->exp, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstReturnStmt *new_ast_return_stmt() {
  AstReturnStmt *node = malloc(sizeof(AstReturnStmt));
  node->base.type = AST_RETURN_STMT;
  node->base.dump = (DumpFunc)ast_return_stmt_dump;
  node->exp = NULL;
  return node;
}

void ast_const_def_dump(AstConstDef *node, int indent) {
  printf("%*sConstDef: {\n", indent, " ");
  printf("%*s  name: %s,\n", indent, " ", node->name);
  printf("%*s  exp: ", indent, " ");
  node->exp->dump((AstBase *)node->exp, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstConstDef *new_ast_const_def() {
  AstConstDef *node = malloc(sizeof(AstConstDef));
  node->base.type = AST_CONST_DEF;
  node->base.dump = (DumpFunc)ast_const_def_dump;
  node->name = NULL;
  node->exp = NULL;
  return node;
}

void ast_const_decl_dump(AstConstDecl *node, int indent) {
  printf("%*sConstDecl: {\n", indent, " ");
  AstConstDef *def = node->def;
  while (def) {
    def->base.dump((AstBase *)def, indent + 2);
    def = def->next;
    if (def) {
      printf(",\n");
    }
  }
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstConstDecl *new_ast_const_decl() {
  AstConstDecl *node = malloc(sizeof(AstConstDecl));
  node->base.type = AST_CONST_DECL;
  node->base.dump = (DumpFunc)ast_const_decl_dump;
  node->def = NULL;
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
  AstStmt *stmt = node->stmt;
  while (stmt) {
    stmt->dump((AstBase *)stmt, indent + 2);
    printf(",\n");
    stmt = stmt->next;
  }
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
  printf("%*s  func_type: %s,\n", indent, " ",
         btype_to_string(node->func_type));
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
  node->func_type = BType_UNKNOWN;
  node->ident = NULL;
  node->block = NULL;
  return node;
}

void ast_comp_unit_dump(AstCompUnit *node, int indent) {
  printf("%*sCompUnit: {\n", indent, " ");
  printf("%*s  func_def: ", indent, " ");
  node->func_def->base.dump((AstBase *)node->func_def, indent + 2);
  printf(",\n");
  printf("%*s}\n", indent, " ");
}

AstCompUnit *new_ast_comp_unit() {
  AstCompUnit *node = malloc(sizeof(AstCompUnit));
  node->base.type = AST_COMP_UNIT;
  node->base.dump = (DumpFunc)ast_comp_unit_dump;
  node->func_def = NULL;
  return node;
}