#include "riscv.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "koopa.h"

static FILE *fp;

static void outputf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void outputf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  //   vprintf(fmt, args);
  vfprintf(fp, fmt, args);
  va_end(args);
}

static void visit_koopa_raw_return(const koopa_raw_return_t ret);
static void visit_koopa_raw_integer(const koopa_raw_integer_t n);
static void visit_koopa_raw_value(const koopa_raw_value_t value);
static void visit_koopa_raw_basic_block(const koopa_raw_basic_block_t block);
static void visit_koopa_raw_function(const koopa_raw_function_t func);
static void visit_koopa_raw_slice(const koopa_raw_slice_t slice);
static void visit_koopa_raw_program(const koopa_raw_program_t program);

static void visit_koopa_raw_return(const koopa_raw_return_t ret) {
  visit_koopa_raw_value(ret.value);
  outputf("  mv a0, t0\n");
  outputf("  ret\n");
}

static void visit_koopa_raw_integer(const koopa_raw_integer_t n) {
  outputf("  li t0, %d\n", n.value);
}

static void visit_koopa_raw_value(const koopa_raw_value_t value) {
  koopa_raw_value_kind_t kind = value->kind;
  switch (kind.tag) {
  case KOOPA_RVT_RETURN:
    visit_koopa_raw_return(kind.data.ret);
    break;
  case KOOPA_RVT_INTEGER:
    visit_koopa_raw_integer(kind.data.integer);
    break;
  default:
    printf("visit_koopa_raw_value unknown kind: %d\n", kind.tag);
    assert(false);
  }
}

static void visit_koopa_raw_basic_block(const koopa_raw_basic_block_t block) {
  visit_koopa_raw_slice(block->insts);
}

static void visit_koopa_raw_function(const koopa_raw_function_t func) {
  outputf("%s:\n", func->name + 1); // + 1 是为了跳过函数名前的 @
  visit_koopa_raw_slice(func->bbs);
}

static void visit_koopa_raw_slice(const koopa_raw_slice_t slice) {
  for (size_t i = 0; i < slice.len; i++) {
    const void *ptr = slice.buffer[i];
    // 根据 slice 的 kind 决定将 ptr 视作何种元素
    switch (slice.kind) {
    case KOOPA_RSIK_FUNCTION:
      // + 1 是为了跳过函数名前的 @
      outputf("  .globl %s\n", ((koopa_raw_function_t)ptr)->name + 1);
      visit_koopa_raw_function(ptr);
      break;
    case KOOPA_RSIK_BASIC_BLOCK:
      visit_koopa_raw_basic_block(ptr);
      break;
    case KOOPA_RSIK_VALUE:
      visit_koopa_raw_value(ptr);
      break;
    default:
      printf("visit_koopa_raw_slice unknown kind: %d\n", slice.kind);
      assert(false);
    }
  }
}

static void visit_koopa_raw_program(const koopa_raw_program_t program) {
  outputf("  .text\n");
  visit_koopa_raw_slice(program.values);
  visit_koopa_raw_slice(program.funcs);
}

void riscv_codegen(const char *ir, const char *output_file) {
  fp = fopen(output_file, "w");
  if (fp == NULL) {
    fprintf(stderr, "无法打开文件 %s\n", output_file);
    exit(1);
  }
  // 解析字符串, 得到 Koopa IR 程序
  koopa_program_t program;
  koopa_error_code_t ret = koopa_parse_from_string(ir, &program);
  assert(ret == KOOPA_EC_SUCCESS); // 确保解析时没有出错
  // 创建一个 raw program builder, 用来构建 raw program
  koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
  // 将 Koopa IR 程序转换为 raw program
  koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
  // 释放 Koopa IR 程序占用的内存
  koopa_delete_program(program);

  // 处理 raw program
  visit_koopa_raw_program(raw);

  // 处理完成, 释放 raw program builder 占用的内存
  // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
  // 所以不要在 raw program 处理完毕之前释放 builder
  koopa_delete_raw_program_builder(builder);

  fclose(fp);
}