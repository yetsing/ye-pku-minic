#include "koopa_ir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

FILE *fp = NULL;
int symbol_index = 0;

static void outputf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void outputf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);
}

// 返回符号，表示表达式的结果
static char *exp_symbol(AstExp *exp) {
  size_t size = 32;
  char *buf = malloc(size);
  if (exp->type == AST_UNARY_EXP && ((AstUnaryExp *)exp)->op == '+') {
    free(buf);
    return exp_symbol(((AstUnaryExp *)exp)->operand);
  } else if (exp->type == AST_NUMBER) {
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
              exp_symbol(unary_exp->operand));
      symbol_index++;
      break;
    }
    case '!': {
      outputf("  %%%d = eq %s, 0\n", symbol_index,
              exp_symbol(unary_exp->operand));
      symbol_index++;
      break;
    }
    case '+':
      // nothing to do
      break;
    }
    break;
  }
  case AST_BINARY_EXP: {
    AstBinaryExp *binary_exp = (AstBinaryExp *)exp;
    codegen_exp(binary_exp->lhs);
    char *lhs = exp_symbol(binary_exp->lhs);
    codegen_exp(binary_exp->rhs);
    char *rhs = exp_symbol(binary_exp->rhs);
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
    fprintf(stderr, "未知的表达式类型\n");
    exit(1);
  }
}

static void codegen_stmt(AstStmt *stmt) {
  codegen_exp(stmt->exp);
  outputf("  ret %s\n", exp_symbol(stmt->exp));
}

static void codegen_block(AstBlock *block) {
  outputf("%%entry:\n");
  codegen_stmt(block->stmt);
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

void koopa_ir_codegen(AstCompUnit *comp_unit, const char *output_file) {
  fp = fopen(output_file, "w");
  if (fp == NULL) {
    fprintf(stderr, "无法打开文件 %s\n", output_file);
    exit(1);
  }

  codegen_comp_unit(comp_unit);
  fclose(fp);
}