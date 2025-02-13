#include "riscv.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "koopa.h"

static FILE *fp;
static const char *register_names[] = {
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a0",
    "a1", "a2", "a3", "a4", "a5", "a6", "a7",
};
static int register_index = 0;

__attribute__((format(printf, 1, 2))) static void outputf(const char *fmt, ...);
static void outputf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  // vprintf(fmt, args);
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
static void visit_koopa_raw_binary(const koopa_raw_binary_t binary);

static bool is_zero(const koopa_raw_value_t value) {
  if (value->kind.tag == KOOPA_RVT_INTEGER) {
    return value->kind.data.integer.value == 0;
  }
  return false;
}

static void visit_koopa_raw_return(const koopa_raw_return_t ret) {
  visit_koopa_raw_value(ret.value);
  outputf("  mv a0, %s\n", register_names[register_index - 1]);
  outputf("  ret\n");
}

static void visit_koopa_raw_integer(const koopa_raw_integer_t n) {
  assert(register_index < sizeof(register_names) / sizeof(register_names[0]));
  outputf("  li %s, %d\n", register_names[register_index], n.value);
  register_index++;
}

static void visit_koopa_raw_binary(const koopa_raw_binary_t binary) {
  // 如果 lhs 或 rhs 是 0, 则不需要分配寄存器，直接使用 x0 即可
  const char *lhs_register = "x0";
  if (!is_zero(binary.lhs)) {
    visit_koopa_raw_value(binary.lhs);
    lhs_register = register_names[register_index - 1];
  }
  const char *rhs_register = "x0";
  if (!is_zero(binary.rhs)) {
    visit_koopa_raw_value(binary.rhs);
    rhs_register = register_names[register_index - 1];
  }
  // 结果寄存器尽量服用 lhs 或 rhs 的寄存器
  const char *result_register = "";
  if (strcmp(rhs_register, "x0") != 0) {
    result_register = rhs_register;
  } else if (strcmp(lhs_register, "x0") != 0) {
    result_register = lhs_register;
  } else {
    assert(register_index < sizeof(register_names) / sizeof(register_names[0]));
    result_register = register_names[register_index];
    register_index++;
  }
  switch (binary.op) {
  case KOOPA_RBO_SUB:
    outputf("  sub %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  case KOOPA_RBO_ADD:
    outputf("  add %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  case KOOPA_RBO_MUL:
    outputf("  mul %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  case KOOPA_RBO_DIV:
    outputf("  div %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  case KOOPA_RBO_MOD:
    outputf("  rem %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  case KOOPA_RBO_EQ: {
    outputf("  xor %s, %s, %s\n", result_register, lhs_register, rhs_register);
    outputf("  seqz %s, %s\n", result_register, result_register);
    break;
  }
  case KOOPA_RBO_NOT_EQ: {
    outputf("  xor %s, %s, %s\n", result_register, lhs_register, rhs_register);
    outputf("  snez %s, %s\n", result_register, result_register);
    break;
  }
  case KOOPA_RBO_LT: {
    outputf("  slt %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  }
  case KOOPA_RBO_LE: {
    outputf("  slt %s, %s, %s\n", result_register, rhs_register, lhs_register);
    outputf("  xori %s, %s, 1\n", result_register, result_register);
    break;
  }
  case KOOPA_RBO_GT: {
    outputf("  slt %s, %s, %s\n", result_register, rhs_register, lhs_register);
    break;
  }
  case KOOPA_RBO_GE: {
    outputf("  slt %s, %s, %s\n", result_register, lhs_register, rhs_register);
    outputf("  xori %s, %s, 1\n", result_register, result_register);
    break;
  }
  case KOOPA_RBO_AND:
    outputf("  and %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  case KOOPA_RBO_OR:
    outputf("  or %s, %s, %s\n", result_register, lhs_register, rhs_register);
    break;
  default:
    printf("visit_koopa_raw_binary unknown op: %d\n", binary.op);
    assert(false);
  }
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
  case KOOPA_RVT_BINARY: {
    visit_koopa_raw_binary(kind.data.binary);
    break;
  }
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
    case KOOPA_RSIK_VALUE: {
      const koopa_raw_value_t value = ptr;
      if (value->used_by.len > 0) {
        // 在处理 used_by 的时候，会将这个 value 也处理
        // 这里做个判断，避免重复处理
        // 比如说这两条 IR 指令:
        // %1 = add 1, 2
        // ret %1
        // basic block 里面会有 %1 = add 1, 2 和 ret %1 两条指令
        // ret 指令里面也会指向 %1 = add 1, 2
        // 如果不做判断，会导致 %1 = add 1, 2 被处理两次
        break;
      }
      visit_koopa_raw_value(ptr);
      break;
    }
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