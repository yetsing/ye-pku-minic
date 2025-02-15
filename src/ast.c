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
  case AST_STMT:
    return "stmt";
  case AST_EXP_STMT:
    return "exp_stmt";
  case AST_RETURN_STMT:
    return "return_stmt";
  case AST_ASSIGN_STMT:
    return "assign_stmt";
  case AST_CONST_DEF:
    return "const_def";
  case AST_CONST_DECL:
    return "const_decl";
  case AST_VAR_DEF:
    return "var_def";
  case AST_VAR_DECL:
    return "var_decl";
  case AST_IDENTIFIER:
    return "identifier";
  case AST_BLOCK:
    return "block";
  case AST_FUNC_DEF:
    return "func_def";
  case AST_COMP_UNIT:
    return "comp_unit";
  case AST_EMPTY_STMT:
    return "empty_stmt";
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
  AstNumber *node = calloc(1, sizeof(AstNumber));
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
  AstUnaryExp *node = calloc(1, sizeof(AstUnaryExp));
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
  AstBinaryExp *node = calloc(1, sizeof(AstBinaryExp));
  node->base.type = AST_BINARY_EXP;
  node->base.dump = (DumpFunc)ast_binary_exp_dump;
  node->op = 0;
  node->lhs = NULL;
  node->rhs = NULL;
  return node;
}

void ast_empty_stmt_dump(AstEmptyStmt *node, int indent) {
  printf("%*sEmptyStmt", indent, " ");
}

AstEmptyStmt *new_ast_empty_stmt() {
  AstEmptyStmt *node = calloc(1, sizeof(AstEmptyStmt));
  node->base.type = AST_EMPTY_STMT;
  node->base.dump = (DumpFunc)ast_empty_stmt_dump;
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
  AstExpStmt *node = calloc(1, sizeof(AstExpStmt));
  node->base.type = AST_EXP_STMT;
  node->base.dump = (DumpFunc)ast_exp_stmt_dump;
  node->exp = NULL;
  return node;
}

void ast_return_stmt_dump(AstReturnStmt *node, int indent) {
  printf("%*sReturnStmt: {\n", indent, " ");
  if (node->exp) {
    printf("%*s  exp: ", indent, " ");
    node->exp->dump((AstBase *)node->exp, indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstReturnStmt *new_ast_return_stmt() {
  AstReturnStmt *node = calloc(1, sizeof(AstReturnStmt));
  node->base.type = AST_RETURN_STMT;
  node->base.dump = (DumpFunc)ast_return_stmt_dump;
  node->exp = NULL;
  return node;
}

void ast_assign_stmt_dump(AstAssignStmt *node, int indent) {
  printf("%*sAssignStmt: {\n", indent, " ");
  printf("%*s  lhs: ", indent, " ");
  node->lhs->dump((AstBase *)node->lhs, indent + 2);
  printf(",\n");
  printf("%*s  exp: ", indent, " ");
  node->exp->dump((AstBase *)node->exp, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstAssignStmt *new_ast_assign_stmt() {
  AstAssignStmt *node = calloc(1, sizeof(AstAssignStmt));
  node->base.type = AST_ASSIGN_STMT;
  node->base.dump = (DumpFunc)ast_assign_stmt_dump;
  node->lhs = NULL;
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
  AstConstDef *node = calloc(1, sizeof(AstConstDef));
  node->base.type = AST_CONST_DEF;
  node->base.dump = (DumpFunc)ast_const_def_dump;
  node->name = NULL;
  node->exp = NULL;
  node->next = NULL;
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
  AstConstDecl *node = calloc(1, sizeof(AstConstDecl));
  node->base.type = AST_CONST_DECL;
  node->base.dump = (DumpFunc)ast_const_decl_dump;
  node->def = NULL;
  return node;
}

void ast_var_def_dump(AstVarDef *node, int indent) {
  printf("%*sVarDef: {\n", indent, " ");
  printf("%*s  name: %s,\n", indent, " ", node->name);
  if (node->exp) {
    printf("%*s  exp: ", indent, " ");
    node->exp->dump((AstBase *)node->exp, indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstVarDef *new_ast_var_def() {
  AstVarDef *node = calloc(1, sizeof(AstVarDef));
  node->base.type = AST_VAR_DEF;
  node->base.dump = (DumpFunc)ast_var_def_dump;
  node->name = NULL;
  node->exp = NULL;
  return node;
}

void ast_var_decl_dump(AstVarDecl *node, int indent) {
  printf("%*sVarDecl: {\n", indent, " ");
  AstVarDef *def = node->def;
  while (def) {
    def->base.dump((AstBase *)def, indent + 2);
    printf(",\n");
    def = def->next;
  }
  printf("%*s}", indent, " ");
}

AstVarDecl *new_ast_var_decl() {
  AstVarDecl *node = calloc(1, sizeof(AstVarDecl));
  node->base.type = AST_VAR_DECL;
  node->base.dump = (DumpFunc)ast_var_decl_dump;
  node->type = BType_UNKNOWN;
  node->def = NULL;
  return node;
}

void ast_identifier_dump(AstIdentifier *node, int indent) {
  printf("%s", node->name);
}

AstIdentifier *new_ast_identifier() {
  AstIdentifier *node = calloc(1, sizeof(AstIdentifier));
  node->base.type = AST_IDENTIFIER;
  node->base.dump = (DumpFunc)ast_identifier_dump;
  node->name = NULL;
  return node;
}

void ast_block_dump(AstBlock *node, int indent) {
  printf("%*sBlock: {\n", indent, " ");
  AstStmt *stmt = node->stmt;
  while (stmt) {
    stmt->dump((AstBase *)stmt, indent + 2);
    printf(",\n");
    stmt = stmt->next;
  }
  printf("%*s}", indent, " ");
}

AstBlock *new_ast_block() {
  AstBlock *node = calloc(1, sizeof(AstBlock));
  node->base.type = AST_BLOCK;
  node->base.dump = (DumpFunc)ast_block_dump;
  node->stmt = NULL;
  return node;
}

void ast_func_def_dump(AstFuncDef *node, int indent) {
  printf("%*sFuncDef: {\n", indent, " ");
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
  AstFuncDef *node = calloc(1, sizeof(AstFuncDef));
  node->base.type = AST_FUNC_DEF;
  node->base.dump = (DumpFunc)ast_func_def_dump;
  node->func_type = BType_UNKNOWN;
  node->ident = NULL;
  node->block = NULL;
  return node;
}

void ast_comp_unit_dump(AstCompUnit *node, int indent) {
  printf("%*sCompUnit: {\n", indent, indent > 0 ? " " : "");
  printf("%*s  func_def: ", indent, indent > 0 ? " " : "");
  node->func_def->base.dump((AstBase *)node->func_def, indent + 2);
  printf(",\n");
  printf("%*s}\n", indent, indent > 0 ? " " : "");
}

AstCompUnit *new_ast_comp_unit() {
  AstCompUnit *node = calloc(1, sizeof(AstCompUnit));
  node->base.type = AST_COMP_UNIT;
  node->base.dump = (DumpFunc)ast_comp_unit_dump;
  node->func_def = NULL;
  return node;
}