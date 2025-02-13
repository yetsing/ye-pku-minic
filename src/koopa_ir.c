#include "koopa_ir.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "utils.h"

FILE *fp = NULL;
int symbol_index = 0;

static void outputf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void outputf(const char *fmt, ...) {
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
  Symbol *next; // 指向下一个符号
} Symbol;

static Symbol symbol_table_head = {NULL, false, 0, NULL};

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

static int eval_symbol(const char *name) {
  Symbol *symbol = find_symbol(name);
  if (symbol == NULL) {
    fprintf(stderr, "未定义的符号 %s\n", name);
    exit(1);
  }
  if (!symbol->is_const_value) {
    fprintf(stderr, "符号 %s 不是常量\n", name);
    exit(1);
  }
  return symbol->value;
}

// #region 优化 AST

AstExp *optimize_exp(AstExp *exp);

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
      Symbol *symbol = malloc(sizeof(Symbol));
      symbol->name = def->name;
      symbol->is_const_value = true;
      symbol->value = value;
      symbol->next = symbol_table_head.next;
      symbol_table_head.next = symbol;
    }
    def = def->next;
  }
}

// 优化 AST，工作包括：
//  - 移除一元加法表达式
//  - 常量计算，例如 1 + 2 -> 3
void optimize_comp_unit(AstCompUnit *comp_unit) {
  AstFuncDef *func_def = comp_unit->func_def;
  AstBlock *block = func_def->block;
  AstStmt head;
  AstStmt *tail = &head;
  AstStmt *stmt = block->stmt;
  while (stmt) {
    switch (stmt->type) {
    case AST_CONST_DECL: {
      optimize_const_decl((AstConstDecl *)stmt);
      // 常量声明不需要加入到新的 block 中
      break;
    }
    case AST_RETURN_STMT: {
      AstReturnStmt *return_stmt = (AstReturnStmt *)stmt;
      return_stmt->exp = optimize_exp(return_stmt->exp);
      tail->next = stmt;
      tail = stmt;
      break;
    }
    default:
      tail->next = stmt;
      tail = stmt;
      break;
    }
    stmt = stmt->next;
  }
  block->stmt = head.next;
}

// #endregion

// #region 生成 IR

// 返回符号，表示表达式的结果
static char *exp_sign(AstExp *exp) {
  size_t size = 32;
  char *buf = malloc(size);
  if (exp->type == AST_NUMBER) {
    snprintf(buf, size, "%d", ((AstNumber *)exp)->number);
  } else {
    snprintf(buf, size, "%%%d", symbol_index - 1);
  }
  return buf;
}

static void codegen_exp(AstExp *exp) {
  switch (exp->type) {
  case AST_UNARY_EXP: {
    AstUnaryExp *unary_exp = (AstUnaryExp *)exp;
    codegen_exp(unary_exp->operand);
    switch (unary_exp->op) {
    case '-': {
      outputf("  %%%d = sub 0, %s\n", symbol_index,
              exp_sign(unary_exp->operand));
      symbol_index++;
      break;
    }
    case '!': {
      outputf("  %%%d = eq %s, 0\n", symbol_index,
              exp_sign(unary_exp->operand));
      symbol_index++;
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
      outputf("  %%%d = add %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_SUB: {
      outputf("  %%%d = sub %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_MUL: {
      outputf("  %%%d = mul %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_DIV: {
      outputf("  %%%d = div %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_MOD: {
      outputf("  %%%d = mod %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_EQ: {
      outputf("  %%%d = eq %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_NE: {
      outputf("  %%%d = ne %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_LT: {
      outputf("  %%%d = lt %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_LE: {
      outputf("  %%%d = le %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_GT: {
      outputf("  %%%d = gt %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_GE: {
      outputf("  %%%d = ge %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      break;
    }
    case BinaryOpType_AND: {
      // a && b
      // r1 = a != 0
      // r2 = b != 0
      // r3 = r1 & r2
      outputf("  %%%d = ne %s, 0\n", symbol_index, lhs);
      symbol_index++;
      outputf("  %%%d = ne %s, 0\n", symbol_index, rhs);
      symbol_index++;
      outputf("  %%%d = and %%%d, %%%d\n", symbol_index, symbol_index - 2,
              symbol_index - 1);
      symbol_index++;
      break;
    }
    case BinaryOpType_OR: {
      // a || b
      // r1 = a | b
      // r2 = r1 != 0
      outputf("  %%%d = or %s, %s\n", symbol_index, lhs, rhs);
      symbol_index++;
      outputf("  %%%d = ne %%%d, 0\n", symbol_index, symbol_index - 1);
      symbol_index++;
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
  default:
    fprintf(stderr, "未知的表达式类型 %s\n", ast_type_to_string(exp->type));
    exit(1);
  }
}

static void codegen_return_stmt(AstReturnStmt *stmt) {
  codegen_exp(stmt->exp);
  outputf("  ret %s\n", exp_sign(stmt->exp));
}

static void codegen_stmt(AstStmt *stmt) {
  switch (stmt->type) {
  case AST_RETURN_STMT:
    codegen_return_stmt((AstReturnStmt *)stmt);
    break;
  case AST_CONST_DECL:
    // 常量声明在优化阶段已经处理了
    break;
  default:
    fprintf(stderr, "未知的语句类型 %s\n", ast_type_to_string(stmt->type));
    exit(1);
  }
}

static void codegen_block(AstBlock *block) {
  outputf("%%entry:\n");
  AstStmt *stmt = block->stmt;
  while (stmt) {
    codegen_stmt(stmt);
    stmt = stmt->next;
  }
}

static void codegen_func_def(AstFuncDef *func_def) {
  outputf("fun @%s(): i32 {\n", func_def->ident->name);
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
  printf("    === 优化后的 AST ===\n");
  comp_unit->base.dump((AstBase *)comp_unit, 4);
  printf("\n");
  // 生成 IR
  codegen_comp_unit(comp_unit);
  fclose(fp);
}