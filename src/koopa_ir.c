#include "koopa_ir.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "utils.h"

FILE *fp = NULL;
// IR 里面 %0, %1, %2 等符号的索引计数
int temp_sign_index = 0;
// if 计数，用来生成唯一的标签
int if_index = 0;
bool output_ret_inst = false;

static bool starts_with(const char *str, const char *prefix) {
  // 移除前面的空格
  while (*str == ' ') {
    str++;
  }
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

static void outputf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void outputf(const char *fmt, ...) {
  output_ret_inst = starts_with(fmt, "ret");
  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);
}

typedef struct Symbol Symbol;
typedef struct Symbol {
  const char *name;
  bool is_const_value;
  int value;
  int level;    // 符号的作用域
  int index;    // 符号的计数，用来生成唯一的符号名
  Symbol *next; // 指向下一个符号
} Symbol;

static Symbol symbol_table_head = {NULL, false, 0, 0, 0, NULL};

static void reset_symbol_table() {
  Symbol *symbol = symbol_table_head.next;
  while (symbol) {
    Symbol *next = symbol->next;
    free(symbol);
    symbol = next;
  }
  symbol_table_head.name = NULL;
  symbol_table_head.is_const_value = false;
  symbol_table_head.value = 0;
  symbol_table_head.level = 0;
  symbol_table_head.index = 0;
  symbol_table_head.next = NULL;
}

static Symbol *find_symbol(const char *name) {
  Symbol *symbol = symbol_table_head.next;
  while (symbol) {
    if (strcmp(symbol->name, name) == 0) {
      return symbol;
    }
    symbol = symbol->next;
  }
  return NULL;
}

static Symbol *new_symbol(const char *name) {
  Symbol *found = find_symbol(name);
  if (found != NULL && found->level == symbol_table_head.level) {
    fprintf(stderr, "符号 %s 已经存在\n", name);
    exit(1);
  }
  Symbol *symbol = malloc(sizeof(Symbol));
  symbol->name = name;
  symbol->is_const_value = false;
  symbol->value = 0;
  symbol->level = symbol_table_head.level;
  symbol->next = symbol_table_head.next;
  symbol->index = symbol_table_head.index++;
  symbol_table_head.next = symbol;
  return symbol;
}

static const char *symbol_unique_name(Symbol *symbol) {
  size_t size = strlen(symbol->name) + 32;
  char *buf = malloc(size);
  snprintf(buf, size, "@%s_%d_%d", symbol->name, symbol->level, symbol->index);
  return buf;
}

static int eval_symbol(const char *name) {
  Symbol *symbol = find_symbol(name);
  if (symbol == NULL) {
    fatalf("eval 未定义的符号 %s\n", name);
  }
  if (!symbol->is_const_value) {
    fprintf(stderr, "符号 %s 不是常量\n", name);
    exit(1);
  }
  return symbol->value;
}

static void enter_scope() { symbol_table_head.level++; }

static void leave_scope() {
  Symbol *symbol = symbol_table_head.next;
  while (symbol) {
    Symbol *next = symbol->next;
    if (symbol->level == symbol_table_head.level) {
      free(symbol);
    } else {
      break;
    }
    symbol = next;
  }
  symbol_table_head.next = symbol;
  symbol_table_head.level--;
}

// #region 优化 AST

AstExp *optimize_exp(AstExp *exp);
void optimize_block(AstBlock *block);
void optimize_stmt(AstStmt *stmt);

bool is_const_exp(AstExp *exp) {
  switch (exp->type) {
  case AST_NUMBER:
    return true;
  case AST_IDENTIFIER: {
    AstIdentifier *ident = (AstIdentifier *)exp;
    Symbol *symbol = find_symbol(ident->name);
    return symbol != NULL && symbol->is_const_value;
  }
  case AST_UNARY_EXP: {
    AstUnaryExp *unary_exp = (AstUnaryExp *)exp;
    return is_const_exp(unary_exp->operand);
  }
  case AST_BINARY_EXP: {
    AstBinaryExp *binary_exp = (AstBinaryExp *)exp;
    return is_const_exp(binary_exp->lhs) && is_const_exp(binary_exp->rhs);
  }
  default:
    return false;
  }
  return false;
}

int eval_const_exp(AstExp *exp) {
  switch (exp->type) {
  case AST_NUMBER:
    return ((AstNumber *)exp)->number;
  case AST_IDENTIFIER: {
    AstIdentifier *ident = (AstIdentifier *)exp;
    return eval_symbol(ident->name);
  }
  case AST_UNARY_EXP: {
    AstUnaryExp *unary_exp = (AstUnaryExp *)exp;
    int operand = eval_const_exp(unary_exp->operand);
    switch (unary_exp->op) {
    case '-':
      return -operand;
    case '!':
      return !operand;
    case '+':
      return operand;
    default:
      fprintf(stderr, "未知的一元运算符 %c\n", unary_exp->op);
      exit(1);
    }
  }
  case AST_BINARY_EXP: {
    AstBinaryExp *binary_exp = (AstBinaryExp *)exp;
    int lhs = eval_const_exp(binary_exp->lhs);
    int rhs = eval_const_exp(binary_exp->rhs);
    switch (binary_exp->op) {
    case BinaryOpType_ADD:
      return lhs + rhs;
    case BinaryOpType_SUB:
      return lhs - rhs;
    case BinaryOpType_MUL:
      return lhs * rhs;
    case BinaryOpType_DIV:
      return lhs / rhs;
    case BinaryOpType_MOD:
      return lhs % rhs;
    case BinaryOpType_EQ:
      return lhs == rhs;
    case BinaryOpType_NE:
      return lhs != rhs;
    case BinaryOpType_LT:
      return lhs < rhs;
    case BinaryOpType_LE:
      return lhs <= rhs;
    case BinaryOpType_GT:
      return lhs > rhs;
    case BinaryOpType_GE:
      return lhs >= rhs;
    case BinaryOpType_AND:
      return lhs && rhs;
    case BinaryOpType_OR:
      return lhs || rhs;
    default:
      fprintf(stderr, "未知的二元运算符\n");
      exit(1);
    }
  }
  default:
    fprintf(stderr, "非常量表达式\n");
    exit(1);
  }
}

AstExp *optimize_unary_exp(AstUnaryExp *unary_exp) {
  unary_exp->operand = optimize_exp(unary_exp->operand);
  if (is_const_exp(unary_exp->operand)) {
    int operand = eval_const_exp((AstExp *)unary_exp);
    AstNumber *number = new_ast_number();
    number->number = operand;
    return (AstExp *)number;
  }
  if (unary_exp->op == '+') {
    return unary_exp->operand;
  }
  return (AstExp *)unary_exp;
}

AstExp *optimize_binary_exp(AstBinaryExp *binary_exp) {
  binary_exp->lhs = optimize_exp(binary_exp->lhs);
  binary_exp->rhs = optimize_exp(binary_exp->rhs);
  if (is_const_exp((AstExp *)binary_exp)) {
    AstNumber *number = new_ast_number();
    number->number = eval_const_exp((AstExp *)binary_exp);
    return (AstExp *)number;
  }
  return (AstExp *)binary_exp;
}

AstExp *optimize_exp(AstExp *exp) {
  switch (exp->type) {
  case AST_UNARY_EXP: {
    return optimize_unary_exp((AstUnaryExp *)exp);
  }
  case AST_BINARY_EXP: {
    return optimize_binary_exp((AstBinaryExp *)exp);
  }
  case AST_IDENTIFIER: {
    if (is_const_exp(exp)) {
      int value = eval_const_exp(exp);
      AstNumber *number = new_ast_number();
      number->number = value;
      return (AstExp *)number;
    }
    return exp;
  }
  default:
    return exp;
  }
}

void optimize_const_decl(AstConstDecl *decl) {
  AstConstDef *def = decl->def;
  while (def) {
    if (is_const_exp(def->exp)) {
      int value = eval_const_exp(def->exp);
      Symbol *symbol = new_symbol(def->name);
      symbol->is_const_value = true;
      symbol->value = value;
    }
    def = def->next;
  }
}

void optimize_stmt(AstStmt *stmt) {
  switch (stmt->type) {
  case AST_RETURN_STMT: {
    AstReturnStmt *return_stmt = (AstReturnStmt *)stmt;
    return_stmt->exp = optimize_exp(return_stmt->exp);
    break;
  }
  case AST_EXP_STMT: {
    AstExpStmt *exp_stmt = (AstExpStmt *)stmt;
    exp_stmt->exp = optimize_exp(exp_stmt->exp);
    break;
  }
  case AST_ASSIGN_STMT: {
    AstAssignStmt *assign_stmt = (AstAssignStmt *)stmt;
    assign_stmt->exp = optimize_exp(assign_stmt->exp);
    break;
  }
  case AST_EMPTY_STMT:
    // nothing to do
    break;
  case AST_BLOCK:
    optimize_block((AstBlock *)stmt);
    break;
  case AST_IF_STMT: {
    AstIfStmt *if_stmt = (AstIfStmt *)stmt;
    if_stmt->condition = optimize_exp(if_stmt->condition);
    optimize_stmt(if_stmt->then);
    if (if_stmt->else_) {
      optimize_stmt(if_stmt->else_);
    }
  }

  default:
    break;
  }
}

void optimize_block(AstBlock *block) {
  AstStmt head;
  head.next = NULL;
  AstStmt *tail = &head;
  AstStmt *stmt = block->stmt;
  enter_scope();
  while (stmt) {
    switch (stmt->type) {
    case AST_CONST_DECL: {
      optimize_const_decl((AstConstDecl *)stmt);
      // 移除常量声明
      break;
    }
    case AST_VAR_DECL: {
      AstVarDecl *var_decl = (AstVarDecl *)stmt;
      AstVarDef *def = var_decl->def;
      while (def) {
        if (def->exp) {
          def->exp = optimize_exp(def->exp);
        }
        def = def->next;
      }
      tail->next = stmt;
      tail = stmt;
      break;
    }
    case AST_EMPTY_STMT:
      // 移除空语句
      break;
    default:
      optimize_stmt(stmt);
      tail->next = stmt;
      tail = stmt;
      break;
    }
    if (stmt->type == AST_RETURN_STMT) {
      // return 之后的语句不会执行到，直接移除
      stmt->next = NULL;
      break;
    }
    stmt = stmt->next;
  }
  leave_scope();
  block->stmt = head.next;
}

// 优化 AST，工作包括：
//  - 移除一元加法表达式
//  - 数字计算，例如 1 + 2 -> 3
//  - 常量替换，例如 const a = 1; const b = a + 2; -> const a = 1; const b = 3;
void optimize_comp_unit(AstCompUnit *comp_unit) {
  AstFuncDef *func_def = comp_unit->func_def;
  optimize_block(func_def->block);

  // 重置符号表
  reset_symbol_table();

  printf("    === 优化后的 AST ===\n");
  comp_unit->base.dump((AstBase *)comp_unit, 4);
  printf("\n");
}

// #endregion

// #region 生成 IR

static void codegen_block(AstBlock *block);
static void codegen_stmt(AstStmt *stmt);

// 返回符号，表示表达式的结果
static char *exp_sign(AstExp *exp) {
  size_t size = 32;
  char *buf = malloc(size);
  if (exp->type == AST_NUMBER) {
    // 数字直接返回数字
    snprintf(buf, size, "%d", ((AstNumber *)exp)->number);
  } else {
    // 其他表达式返回 %0, %1, %2 等符号
    snprintf(buf, size, "%%%d", temp_sign_index - 1);
  }
  return buf;
}

static void codegen_identifier(AstIdentifier *ident) {
  Symbol *symbol = find_symbol(ident->name);
  if (symbol == NULL) {
    fatalf("访问未定义的符号 %s\n", ident->name);
  }
  const char *name = symbol_unique_name(symbol);
  outputf("  %%%d = load %s\n", temp_sign_index, name);
  free((void *)name);
  temp_sign_index++;
}

static void codegen_exp(AstExp *exp) {
  switch (exp->type) {
  case AST_UNARY_EXP: {
    AstUnaryExp *unary_exp = (AstUnaryExp *)exp;
    codegen_exp(unary_exp->operand);
    switch (unary_exp->op) {
    case '-': {
      outputf("  %%%d = sub 0, %s\n", temp_sign_index,
              exp_sign(unary_exp->operand));
      temp_sign_index++;
      break;
    }
    case '!': {
      outputf("  %%%d = eq %s, 0\n", temp_sign_index,
              exp_sign(unary_exp->operand));
      temp_sign_index++;
      break;
    }
    case '+':
      // nothing to do
      fatalf("不应该出现一元加法表达式\n");
      break;
    }
    break;
  }
  case AST_BINARY_EXP: {
    AstBinaryExp *binary_exp = (AstBinaryExp *)exp;
    codegen_exp(binary_exp->lhs);
    char *lhs = exp_sign(binary_exp->lhs);
    codegen_exp(binary_exp->rhs);
    char *rhs = exp_sign(binary_exp->rhs);
    switch (binary_exp->op) {
    case BinaryOpType_ADD: {
      outputf("  %%%d = add %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_SUB: {
      outputf("  %%%d = sub %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_MUL: {
      outputf("  %%%d = mul %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_DIV: {
      outputf("  %%%d = div %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_MOD: {
      outputf("  %%%d = mod %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_EQ: {
      outputf("  %%%d = eq %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_NE: {
      outputf("  %%%d = ne %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_LT: {
      outputf("  %%%d = lt %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_LE: {
      outputf("  %%%d = le %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_GT: {
      outputf("  %%%d = gt %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_GE: {
      outputf("  %%%d = ge %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_AND: {
      // a && b
      // r1 = a != 0
      // r2 = b != 0
      // r3 = r1 & r2
      outputf("  %%%d = ne %s, 0\n", temp_sign_index, lhs);
      temp_sign_index++;
      outputf("  %%%d = ne %s, 0\n", temp_sign_index, rhs);
      temp_sign_index++;
      outputf("  %%%d = and %%%d, %%%d\n", temp_sign_index, temp_sign_index - 2,
              temp_sign_index - 1);
      temp_sign_index++;
      break;
    }
    case BinaryOpType_OR: {
      // a || b
      // r1 = a | b
      // r2 = r1 != 0
      outputf("  %%%d = or %s, %s\n", temp_sign_index, lhs, rhs);
      temp_sign_index++;
      outputf("  %%%d = ne %%%d, 0\n", temp_sign_index, temp_sign_index - 1);
      temp_sign_index++;
      break;
    }

    default:
      fprintf(stderr, "未知的二元运算符 %c\n", binary_exp->op);
      exit(1);
    }
    free(lhs);
    free(rhs);
    break;
  }

  case AST_NUMBER:
    // nothing to do
    break;
  case AST_IDENTIFIER:
    codegen_identifier((AstIdentifier *)exp);
    break;
  default:
    fprintf(stderr, "未知的表达式类型 %s\n", ast_type_to_string(exp->type));
    exit(1);
  }
}

static void codegen_return_stmt(AstReturnStmt *stmt) {
  codegen_exp(stmt->exp);
  outputf("  ret %s\n", exp_sign(stmt->exp));
}

static void codegen_assign_stmt(AstAssignStmt *stmt) {
  if (stmt->lhs->type != AST_IDENTIFIER) {
    fatalf("左值必须是标识符\n");
  }
  AstIdentifier *ident = (AstIdentifier *)stmt->lhs;
  Symbol *symbol = find_symbol(ident->name);
  if (symbol == NULL) {
    fatalf("赋值未定义的符号 %s\n", ident->name);
  }
  codegen_exp(stmt->exp);
  const char *name = symbol_unique_name(symbol);
  outputf("  store %s, %s\n", exp_sign(stmt->exp), name);
  free((void *)name);
}

static void codegen_var_decl(AstVarDecl *decl) {
  AstVarDef *def = decl->def;
  while (def) {
    Symbol *symbol = new_symbol(def->name);
    const char *name = symbol_unique_name(symbol);
    outputf("  %s = alloc i32\n", name);
    if (def->exp) {
      codegen_exp(def->exp);
      outputf("  store %s, %s\n", exp_sign(def->exp), name);
    }
    free((void *)name);
    def = def->next;
  }
}

static void codegen_exp_stmt(AstExpStmt *stmt) { codegen_exp(stmt->exp); }

static void codegen_if_stmt(AstIfStmt *stmt) {
  if_index++;
  int current_if_index = if_index;
  codegen_exp(stmt->condition);
  if (stmt->else_) {
    outputf("  br %s, %%if_then_%d, %%if_else_%d\n", exp_sign(stmt->condition),
            current_if_index, current_if_index);
  } else {
    outputf("  br %s, %%if_then_%d, %%if_end_%d\n", exp_sign(stmt->condition),
            current_if_index, current_if_index);
  }
  outputf("%%if_then_%d:\n", current_if_index);
  codegen_stmt(stmt->then);
  if (!output_ret_inst) {
    // basic block 结尾后面如果是 ret 指令，就不需要 jump
    outputf("  jump %%if_end_%d\n", current_if_index);
  }
  if (stmt->else_) {
    outputf("%%if_else_%d:\n", current_if_index);
    codegen_stmt(stmt->else_);
    if (!output_ret_inst) {
      // basic block 结尾后面如果是 ret 指令，就不需要 jump
      outputf("  jump %%if_end_%d\n", current_if_index);
    }
  }
  outputf("%%if_end_%d:\n", current_if_index);
}

static void codegen_stmt(AstStmt *stmt) {
  switch (stmt->type) {
  case AST_IF_STMT:
    codegen_if_stmt((AstIfStmt *)stmt);
    break;
  case AST_RETURN_STMT:
    codegen_return_stmt((AstReturnStmt *)stmt);
    break;
  case AST_ASSIGN_STMT:
    codegen_assign_stmt((AstAssignStmt *)stmt);
    break;
  case AST_CONST_DECL:
    // 常量声明在优化阶段已经处理了
    fatalf("不应该出现常量声明\n");
    break;
  case AST_VAR_DECL:
    codegen_var_decl((AstVarDecl *)stmt);
    break;
  case AST_BLOCK:
    codegen_block((AstBlock *)stmt);
    break;
  case AST_EXP_STMT:
    codegen_exp_stmt((AstExpStmt *)stmt);
    break;
  case AST_EMPTY_STMT:
    // nothing to do
    break;
  default:
    fatalf("未知的语句类型 %s\n", ast_type_to_string(stmt->type));
  }
}

static void codegen_block(AstBlock *block) {
  AstStmt *stmt = block->stmt;
  enter_scope();
  while (stmt) {
    codegen_stmt(stmt);
    stmt = stmt->next;
  }
  leave_scope();
}

static void codegen_func_def(AstFuncDef *func_def) {
  outputf("fun @%s(): i32 {\n", func_def->ident->name);
  outputf("%%entry:\n");
  codegen_block(func_def->block);
  outputf("}\n");
}

static void codegen_comp_unit(AstCompUnit *comp_unit) {
  if (strcmp(comp_unit->func_def->ident->name, "main") != 0) {
    fprintf(stderr, "入口函数必须是 main\n");
    exit(1);
  }
  codegen_func_def(comp_unit->func_def);
}

// #endregion

void koopa_ir_codegen(AstCompUnit *comp_unit, const char *output_file) {
  fp = fopen(output_file, "w");
  if (fp == NULL) {
    fprintf(stderr, "无法打开文件 %s\n", output_file);
    exit(1);
  }

  // 优化 AST
  optimize_comp_unit(comp_unit);
  // 生成 IR
  codegen_comp_unit(comp_unit);
  fclose(fp);
}