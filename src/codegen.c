#include "codegen.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *fp = NULL;

static void outputf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void outputf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);
}

static void codegen_stmt(AstStmt *stmt) {
  outputf("  ret %d\n", stmt->number->number);
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

void codegen(AstCompUnit *comp_unit, const char *output_file) {
  fp = fopen(output_file, "w");
  if (fp == NULL) {
    fprintf(stderr, "无法打开文件 %s\n", output_file);
    exit(1);
  }

  codegen_comp_unit(comp_unit);
  fclose(fp);
}