#include "ast.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

const char *ast_type_to_string(AstType type) {
  switch (type) {
  case AST_NUMBER:
    return "number";
  case AST_ARRAY_VALUE:
    return "array_value";
  case AST_UNARY_EXP:
    return "unary_exp";
  case AST_BINARY_EXP:
    return "binary_exp";
  case AST_FUNC_CALL:
    return "func_call";
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
  case AST_ARRAY_ACCESS:
    return "array_access";
  case AST_BLOCK:
    return "block";
  case AST_FUNC_DEF:
    return "func_def";
  case AST_COMP_UNIT:
    return "comp_unit";
  case AST_EMPTY_STMT:
    return "empty_stmt";
  case AST_IF_STMT:
    return "if_stmt";
  case AST_WHILE_STMT:
    return "while_stmt";
  case AST_BREAK_STMT:
    return "break_stmt";
  case AST_CONTINUE_STMT:
    return "continue_stmt";
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
  case BType_VOID:
    return "void";
  case BType_POINTER:
    return "pointer";
  case BType_ARRAY_POINTER:
    return "array_pointer";
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

void init_exp_array(ExpArray *array) {
  array->count = 0;
  array->capacity = 10;
  array->elements = calloc(array->capacity, sizeof(AstExp *));
}

void exp_array_add(ExpArray *array, AstExp *element) {
  if (array->count >= array->capacity) {
    array->capacity *= 2;
    array->elements =
        realloc(array->elements, array->capacity * sizeof(AstExp *));
  }
  array->elements[array->count++] = element;
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

void ast_array_value_dump(AstArrayValue *node, int indent) {
  printf("ArrayValue: {\n");
  for (int i = 0; i < node->count; i++) {
    printf("%*s  ", indent, " ");
    node->elements[i]->dump((AstBase *)node->elements[i], indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstArrayValue *new_ast_array_value() {
  AstArrayValue *node = calloc(1, sizeof(AstArrayValue));
  node->base.type = AST_ARRAY_VALUE;
  node->base.dump = (DumpFunc)ast_array_value_dump;
  node->count = 0;
  node->capacity = 10;
  node->elements = calloc(node->capacity, sizeof(AstExp *));
  return node;
}

AstArrayValue *new_ast_array_value2(int count, int capacity) {
  AstArrayValue *node = calloc(1, sizeof(AstArrayValue));
  node->base.type = AST_ARRAY_VALUE;
  node->base.dump = (DumpFunc)ast_array_value_dump;
  node->count = count;
  node->capacity = capacity;
  node->elements = calloc(node->capacity, sizeof(AstExp *));
  return node;
}

void ast_array_value_add(AstArrayValue *array_value, AstExp *element) {
  if (array_value->count >= array_value->capacity) {
    array_value->capacity *= 2;
    array_value->elements = realloc(array_value->elements,
                                    array_value->capacity * sizeof(AstExp *));
  }
  array_value->elements[array_value->count++] = element;
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

void ast_array_access_dump(AstArrayAccess *node, int indent) {
  printf("AstArrayAccess: {\n");
  printf("%*s  name: %s,\n", indent, " ", node->name);
  printf("%*s  indexes: {\n", indent, " ");
  for (int i = 0; i < node->indexes.count; i++) {
    printf("%*s  ", indent + 2, " ");
    node->indexes.elements[i]->dump((AstBase *)node->indexes.elements[i],
                                    indent + 2);
    printf(",\n");
  }
  printf("%*s  },\n", indent, " ");
  printf("%*s}", indent, " ");
}

AstArrayAccess *new_ast_array_access() {
  AstArrayAccess *node = calloc(1, sizeof(AstArrayAccess));
  node->base.type = AST_ARRAY_ACCESS;
  node->base.dump = (DumpFunc)ast_array_access_dump;
  node->name = NULL;
  init_exp_array(&node->indexes);
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

void ast_call_exp_dump(AstFuncCall *node, int indent) {
  printf("CallExp: {\n");
  printf("%*s  ident: %s,\n", indent, " ", node->ident->name);
  for (int i = 0; i < node->count; i++) {
    printf("%*s  arg: ", indent, " ");
    node->args[i]->dump((AstBase *)node->args[i], indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstFuncCall *new_ast_func_call() {
  AstFuncCall *node = calloc(1, sizeof(AstFuncCall));
  node->base.type = AST_FUNC_CALL;
  node->base.dump = (DumpFunc)ast_call_exp_dump;
  node->ident = NULL;
  node->count = 0;
  node->capacity = 10;
  node->args = calloc(node->capacity, sizeof(AstExp *));
  return node;
}

void ast_func_call_add(AstFuncCall *func_call, AstExp *arg) {
  if (func_call->count >= func_call->capacity) {
    func_call->capacity *= 2;
    func_call->args =
        realloc(func_call->args, func_call->capacity * sizeof(AstExp *));
  }
  func_call->args[func_call->count++] = arg;
}

void ast_break_stmt_dump(AstBreakStmt *node, int indent) {
  printf("BreakStmt");
}

AstBreakStmt *new_ast_break_stmt() {
  AstBreakStmt *node = calloc(1, sizeof(AstBreakStmt));
  node->base.type = AST_BREAK_STMT;
  node->base.dump = (DumpFunc)ast_break_stmt_dump;
  return node;
}

void ast_continue_stmt_dump(AstContinueStmt *node, int indent) {
  printf("ContinueStmt");
}

AstContinueStmt *new_ast_continue_stmt() {
  AstContinueStmt *node = calloc(1, sizeof(AstContinueStmt));
  node->base.type = AST_CONTINUE_STMT;
  node->base.dump = (DumpFunc)ast_continue_stmt_dump;
  return node;
}

void ast_while_stmt_dump(AstWhileStmt *node, int indent) {
  printf("WhileStmt: {\n");
  printf("%*s  condition: ", indent, " ");
  node->condition->dump((AstBase *)node->condition, indent + 2);
  printf(",\n");
  printf("%*s  body: ", indent, " ");
  node->body->dump((AstBase *)node->body, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstWhileStmt *new_ast_while_stmt() {
  AstWhileStmt *node = calloc(1, sizeof(AstWhileStmt));
  node->base.type = AST_WHILE_STMT;
  node->base.dump = (DumpFunc)ast_while_stmt_dump;
  node->condition = NULL;
  node->body = NULL;
  return node;
}

void ast_if_stmt_dump(AstIfStmt *node, int indent) {
  printf("IfStmt: {\n");
  printf("%*s  condition: ", indent, " ");
  node->condition->dump((AstBase *)node->condition, indent + 2);
  printf(",\n");
  printf("%*s  then: ", indent, " ");
  node->then->dump((AstBase *)node->then, indent + 2);
  printf(",\n");
  if (node->else_) {
    printf("%*s  else: ", indent, " ");
    node->else_->dump((AstBase *)node->else_, indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstIfStmt *new_ast_if_stmt() {
  AstIfStmt *node = calloc(1, sizeof(AstIfStmt));
  node->base.type = AST_IF_STMT;
  node->base.dump = (DumpFunc)ast_if_stmt_dump;
  node->condition = NULL;
  node->then = NULL;
  node->else_ = NULL;
  return node;
}

void ast_empty_stmt_dump(AstEmptyStmt *node, int indent) {
  printf("EmptyStmt");
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
  printf("ReturnStmt: {\n");
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
  printf("AssignStmt: {\n");
  printf("%*s  lhs: ", indent, indent > 0 ? " " : "");
  node->lhs->dump((AstBase *)node->lhs, indent + 2);
  printf(",\n");
  printf("%*s  exp: ", indent, indent > 0 ? " " : "");
  node->exp->dump((AstBase *)node->exp, indent + 2);
  printf(",\n");
  printf("%*s}", indent, indent > 0 ? " " : "");
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
  printf("ConstDef: {\n");
  printf("%*s  name: %s\n", indent, " ", node->name);
  if (node->dimensions.count > 0) {
    printf("%*s  dimensions: {\n", indent, " ");
    for (int i = 0; i < node->dimensions.count; i++) {
      printf("%*s  ", indent + 2, " ");
      // if (node->dimensions.elements[i] == NULL) {
      //   printf("%*s  <default>,\n", indent + 2, " ");
      //   continue;
      // }
      node->dimensions.elements[i]->dump(
          (AstBase *)node->dimensions.elements[i], indent + 2);
      printf(",\n");
    }
    printf("%*s  },\n", indent, " ");
  }
  printf("%*s  val: ", indent, " ");
  node->val->dump((AstBase *)node->val, indent + 2);
  printf(",\n");
  printf("%*s}", indent, " ");
}

AstConstDef *new_ast_const_def() {
  AstConstDef *node = calloc(1, sizeof(AstConstDef));
  node->base.type = AST_CONST_DEF;
  node->base.dump = (DumpFunc)ast_const_def_dump;
  node->name = NULL;
  node->val = NULL;
  node->next = NULL;
  init_exp_array(&node->dimensions);
  return node;
}

void ast_const_decl_dump(AstConstDecl *node, int indent) {
  printf("ConstDecl: {\n");
  AstConstDef *def = node->def;
  while (def) {
    printf("%*s  ", indent, " ");
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
  printf("VarDef: {\n");
  printf("%*s  name: %s,\n", indent, " ", node->name);
  if (node->dimensions.count > 0) {
    printf("%*s  dimensions: {\n", indent, " ");
    for (int i = 0; i < node->dimensions.count; i++) {
      printf("%*s  ", indent + 2, " ");
      // if (node->dimensions.elements[i] == NULL) {
      //   printf("%*s  <default>,\n", indent + 2, " ");
      //   continue;
      // }
      node->dimensions.elements[i]->dump(
          (AstBase *)node->dimensions.elements[i], indent + 2);
      printf(",\n");
    }
    printf("%*s  },\n", indent, " ");
  }
  if (node->val) {
    printf("%*s  val: ", indent, " ");
    node->val->dump((AstBase *)node->val, indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstVarDef *new_ast_var_def() {
  AstVarDef *node = calloc(1, sizeof(AstVarDef));
  node->base.type = AST_VAR_DEF;
  node->base.dump = (DumpFunc)ast_var_def_dump;
  node->name = NULL;
  node->val = NULL;
  init_exp_array(&node->dimensions);
  return node;
}

void ast_var_decl_dump(AstVarDecl *node, int indent) {
  printf("VarDecl: {\n");
  AstVarDef *def = node->def;
  while (def) {
    printf("%*s  ", indent, " ");
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

void ast_block_dump(AstBlock *node, int indent) {
  printf("Block: {\n");
  AstStmt *stmt = node->stmt;
  while (stmt) {
    printf("%*s  ", indent, " ");
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
  printf("FuncDef: {\n");
  printf("%*s  func_type: %s,\n", indent, " ",
         btype_to_string(node->func_type));
  printf("%*s  ident: ", indent, " ");
  node->ident->base.dump((AstBase *)node->ident, indent + 2);
  printf(",\n");
  printf("%*s  params: ", indent, " ");
  FuncParam *param = node->params;
  while (param) {
    printf("%s %s", btype_to_string(param->type), param->ident->name);
    if (param->dimensions.count > 0) {
      printf("<%d>", param->dimensions.count);
    }
    param = param->next;
    if (param) {
      printf(", ");
    }
  }
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
  node->params = NULL;
  node->param_count = 0;
  return node;
}

void ast_comp_unit_dump(AstCompUnit *node, int indent) {
  printf("%*sCompUnit: {\n", indent, indent > 0 ? " " : "");
  for (int i = 0; i < node->count; i++) {
    printf("%*s  ", indent, " ");
    node->defs[i]->dump(node->defs[i], indent + 2);
    printf(",\n");
  }
  printf("%*s}", indent, " ");
}

AstCompUnit *new_ast_comp_unit() {
  AstCompUnit *node = calloc(1, sizeof(AstCompUnit));
  node->base.type = AST_COMP_UNIT;
  node->base.dump = (DumpFunc)ast_comp_unit_dump;
  node->count = 0;
  node->capacity = 10;
  node->defs = calloc(node->capacity, sizeof(AstBase *));
  return node;
}
void ast_comp_unit_add(AstCompUnit *comp_unit, AstBase *node) {
  if (comp_unit->count >= comp_unit->capacity) {
    comp_unit->capacity *= 2;
    comp_unit->defs =
        realloc(comp_unit->defs, comp_unit->capacity * sizeof(AstBase *));
  }
  comp_unit->defs[comp_unit->count++] = node;
}