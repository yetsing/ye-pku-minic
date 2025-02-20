#include "riscv.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "koopa.h"
#include "utils.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static FILE *fp;

__attribute__((format(printf, 1, 2))) static void outputf(const char *fmt, ...);
static void outputf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  // vprintf(fmt, args);
  vfprintf(fp, fmt, args);
  va_end(args);
}

static int temp_index = -1; // 给临时变量计数用的
// 函数返回时，需要恢复 sp ，所以需要记录栈的大小
// 即生成 ret 指令时，需要将 sp 加上这个大小，恢复栈空间
static size_t stack_size = 0;
// 是否调用了其他函数，如果调用了，需要保存 ra
// 寄存器； ret 指令时，需要恢复 ra 寄存器
static bool has_call = false;

typedef struct Variable Variable;
typedef struct Variable {
  const char *name;
  int offset;
  Variable *next;
  int count; // 计数
} Variable;

static Variable locals = {NULL, 0, NULL, 0}; // 局部变量

static int new_variable(const char *name) {
  assert(name != NULL);
  Variable *var = (Variable *)malloc(sizeof(Variable));
  var->name = name;
  var->offset = locals.next == NULL ? 0 : locals.next->offset + 4;
  var->next = locals.next;
  locals.next = var;
  locals.count++;
  return var->offset;
}

// 给所有局部变量的偏移量增加 offset
static void locals_add_offset(int offset) {
  Variable *var = locals.next;
  while (var != NULL) {
    var->offset += offset;
    var = var->next;
  }
}

static int get_offset(const char *name) {
  Variable *var = locals.next;
  while (var != NULL) {
    if (strcmp(var->name, name) == 0) {
      return var->offset;
    }
    var = var->next;
  }
  fatalf("未找到变量 %s\n", name);
  return -1;
}

static int last_offset(void) {
  // 局部变量是逆序存储的，所以第一个就是最后一个
  Variable *var = locals.next;
  return var == NULL ? 0 : var->offset;
}

static void locals_reset(void) {
  Variable *var = locals.next;
  while (var != NULL) {
    Variable *next = var->next;
    free(var);
    var = next;
  }
  locals.next = NULL;
  locals.count = 0;
}

// 临时变量的偏移量，对应 IR 中的 %0, %1, %2, ...
static int get_temp_offset(void) {
  int last = last_offset();
  int n = (temp_index) * 4 + last;
  // assert(n >= 0 && n < stack_size);
  return n;
}

static void visit_koopa_raw_return(const koopa_raw_return_t ret);
static void visit_koopa_raw_integer(const koopa_raw_integer_t n);
static void visit_koopa_raw_value(const koopa_raw_value_t value);
static void visit_koopa_raw_basic_block(const koopa_raw_basic_block_t block);
static void visit_koopa_raw_function(const koopa_raw_function_t func);
static void visit_koopa_raw_slice(const koopa_raw_slice_t slice);
static void visit_koopa_raw_program(const koopa_raw_program_t program);
static void visit_koopa_raw_binary(const koopa_raw_binary_t binary);
static void visit_koopa_raw_branch(const koopa_raw_branch_t branch);
static void visit_koopa_raw_jump(const koopa_raw_jump_t jump);
static void visit_koopa_raw_call(const koopa_raw_call_t call);

static void visit_koopa_raw_return(const koopa_raw_return_t ret) {
  outputf("\n  # === return ===\n");
  if (ret.value) {
    if (ret.value->kind.tag == KOOPA_RVT_INTEGER) {
      // 如果返回值是常量，直接使用立即数
      outputf("  li a0, %d\n", ret.value->kind.data.integer.value);
    } else {
      visit_koopa_raw_value(ret.value);
      // 从栈中取出返回值
      outputf("  lw a0, %d(sp)\n", get_temp_offset());
    }
  }
  if (has_call) {
    // 如果调用了其他函数，需要恢复 ra 寄存器
    outputf("  lw ra, %zu(sp)\n", stack_size - 4);
  }

  // 函数返回，恢复栈空间 epilogue
  if (stack_size < 2048) {
    outputf("  addi sp, sp, %zu\n", stack_size);
  } else {
    outputf("  li t0, %zu\n", stack_size);
    outputf("  add sp, sp, t0\n");
  }
  outputf("  ret\n");
}

static void visit_koopa_raw_integer(const koopa_raw_integer_t n) {
  outputf("  li t0, %d\n", n.value);
}

static void visit_koopa_raw_binary(const koopa_raw_binary_t binary) {
  outputf("\n  # === binary %d ===\n", binary.op);
  int lhs_offset = -1;
  if (binary.lhs->kind.tag != KOOPA_RVT_INTEGER) {
    visit_koopa_raw_value(binary.lhs);
    lhs_offset = get_temp_offset();
  }
  int rhs_offset = -1;
  if (binary.rhs->kind.tag != KOOPA_RVT_INTEGER) {
    visit_koopa_raw_value(binary.rhs);
    rhs_offset = get_temp_offset();
  }

  const char *lhs_register = "t0";
  if (binary.lhs->kind.tag == KOOPA_RVT_INTEGER) {
    if (binary.lhs->kind.data.integer.value == 0) {
      // 如果 lhs 是 0，直接使用 x0 即可
      lhs_register = "x0";
    } else {
      // 如果 lhs 是常量，直接使用立即数
      outputf("  li t0, %d\n", binary.lhs->kind.data.integer.value);
    }
  } else {
    // 从栈中取出 lhs
    outputf("  lw t0, %d(sp)\n", lhs_offset);
  }

  const char *rhs_register = "t1";
  if (binary.rhs->kind.tag == KOOPA_RVT_INTEGER) {
    if (binary.rhs->kind.data.integer.value == 0) {
      // 如果 rhs 是 0，直接使用 x0 即可
      rhs_register = "x0";
    } else {
      // 如果 rhs 是常量，直接使用立即数
      outputf("  li t1, %d\n", binary.rhs->kind.data.integer.value);
    }
  } else {
    // 从栈中取出 rhs
    outputf("  lw t1, %d(sp)\n", rhs_offset);
  }

  // 目前所有变量都存储在栈上，所以这里可以直接使用 t0 作为结果寄存器
  const char *result_register = "t0";
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
  temp_index++; // 存在返回值，所以栈指针需要移动
  // 将结果存储到栈上
  outputf("  sw %s, %d(sp)\n", result_register, get_temp_offset());
  outputf("  # === binary %d end ===\n", binary.op);
}

static void visit_koopa_raw_load(const koopa_raw_load_t load) {
  temp_index++; // 存在返回值，所以栈指针需要移动
  outputf("  lw t0, %d(sp)\n", get_offset(load.src->name));
  outputf("  sw t0, %d(sp)\n", get_temp_offset());
}

static void visit_koopa_raw_store(const koopa_raw_store_t store) {
  if (store.value->kind.tag == KOOPA_RVT_FUNC_ARG_REF) {
    int index = store.value->kind.data.func_arg_ref.index;
    if (index < 8) {
      // 参数在 a0 ~ a7 中
      outputf("  sw a%d, %d(sp)\n", index, get_offset(store.dest->name));
    } else {
      // 参数在栈上
      outputf("  lw t0, %d(sp)\n", (int)stack_size + (index - 8) * 4);
      // 结果已经在 t0 中了，直接存储到栈上
      // outputf("  lw t0, %d(sp)\n", get_temp_offset());
      outputf("  sw t0, %d(sp)\n", get_offset(store.dest->name));
    }
  } else {
    visit_koopa_raw_value(store.value);
    // 结果已经在 t0 中了，直接存储到栈上
    // outputf("  lw t0, %d(sp)\n", get_temp_offset());
    outputf("  sw t0, %d(sp)\n", get_offset(store.dest->name));
  }
}

static void visit_koopa_raw_branch(const koopa_raw_branch_t branch) {
  outputf("\n  # === branch ===\n");
  visit_koopa_raw_value(branch.cond);
  // 表达式的结果会放在 t0 寄存器，所以不需要额外的操作
  // +1 是为了跳过基本块名前的 %
  outputf("  bnez t0, %s\n", branch.true_bb->name + 1);
  outputf("  j %s\n", branch.false_bb->name + 1);
  outputf("  # === branch end ===\n");
}

static void visit_koopa_raw_jump(const koopa_raw_jump_t jump) {
  outputf("  j %s\n", jump.target->name + 1);
}

static void visit_koopa_raw_call(const koopa_raw_call_t call) {
  outputf("\n  # === call ===\n");
  int *arg_offsets = (int *)malloc(call.args.len * sizeof(int));
  memset(arg_offsets, 0, call.args.len * sizeof(int));
  // 先对调用参数求值
  for (int i = 0; i < 8 && i < call.args.len; i++) {
    const koopa_raw_value_t arg = call.args.buffer[i];
    if (arg->kind.tag == KOOPA_RVT_INTEGER) {
      // 参数是常量，直接使用立即数
    } else {
      visit_koopa_raw_value(arg);
      arg_offsets[i] = get_temp_offset();
    }
  }

  // 再进行参数传递
  for (int i = 0; i < 8 && i < call.args.len; i++) {
    const koopa_raw_value_t arg = call.args.buffer[i];
    if (arg->kind.tag == KOOPA_RVT_INTEGER) {
      // 参数是常量，直接使用立即数
      outputf("  li a%d, %d\n", i, arg->kind.data.integer.value);
    } else {
      outputf("  lw a%d, %d(sp)\n", i, arg_offsets[i]);
    }
  }
  // 如果参数个数大于 8，需要额外的栈空间保存参数
  for (int i = 8; i < call.args.len; i++) {
    const koopa_raw_value_t arg = call.args.buffer[i];
    if (arg->kind.tag == KOOPA_RVT_INTEGER) {
      // 参数是常量，直接使用立即数
      outputf("  li t0, %d\n", arg->kind.data.integer.value);
    } else {
      outputf("  lw t0, %d(sp)\n", arg_offsets[i]);
    }
    outputf("  sw t0, %d(sp)\n", (i - 8) * 4);
  }

  // 调用函数
  outputf("  call %s\n", call.callee->name + 1); // + 1 是为了跳过函数名前的 @
  // 保存返回值
  temp_index++; // 存在返回值，所以栈指针需要移动
  outputf("  sw a0, %d(sp)\n", get_temp_offset());
  outputf("  mv t0, a0\n");
  outputf("  # === call end ===\n");
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
  case KOOPA_RVT_LOAD:
    visit_koopa_raw_load(kind.data.load);
    break;
  case KOOPA_RVT_STORE:
    visit_koopa_raw_store(kind.data.store);
    break;
  case KOOPA_RVT_ALLOC:
    // 分配局部变量
    // nothing to do
    break;
  case KOOPA_RVT_BRANCH:
    visit_koopa_raw_branch(kind.data.branch);
    break;
  case KOOPA_RVT_JUMP:
    visit_koopa_raw_jump(kind.data.jump);
    break;
  case KOOPA_RVT_CALL:
    visit_koopa_raw_call(kind.data.call);
    break;
  }
  default:
    printf("visit_koopa_raw_value unknown kind: %d\n", kind.tag);
    assert(false);
  }
}

static void visit_koopa_raw_basic_block(const koopa_raw_basic_block_t block) {
  if (strcmp(block->name, "%entry") != 0) {
    outputf("\n%s:\n", block->name + 1); // + 1 是为了跳过基本块名前的 %
  }
  visit_koopa_raw_slice(block->insts);
}

static void visit_koopa_raw_function(const koopa_raw_function_t func) {
  temp_index = 0;
  locals_reset();
  /**
    栈帧变量分配情况，从低到高依次为：局部变量、% 开头的临时变量，例如下面这样：
    sp + 12: %2
    sp +  8: %1
    sp +  4: %0
    sp +  0: @x
  */
  // 计算函数需要的栈空间
  stack_size = 0;
  has_call = false;
  int max_call_args = 0;
  for (size_t i = 0; i < func->bbs.len; i++) {
    const koopa_raw_basic_block_t block = func->bbs.buffer[i];
    for (size_t j = 0; j < block->insts.len; j++) {
      const koopa_raw_value_t value = block->insts.buffer[j];
      if (value->ty->tag != KOOPA_RTT_UNIT) {
        // 指令存在返回值，需要分配栈空间
        stack_size += 4;
      }
      if (value->kind.tag == KOOPA_RVT_ALLOC) {
        // 分配局部变量
        new_variable(value->name);
      }
      if (value->kind.tag == KOOPA_RVT_CALL) {
        has_call = true;
        max_call_args = MAX(max_call_args, value->kind.data.call.args.len);
      }
    }
  }
  if (has_call) {
    // 如果函数调用了其他函数，需要额外的栈空间保存 ra 寄存器
    stack_size += 4;
  }
  if (max_call_args > 8) {
    // 如果函数调用的参数个数大于 8，需要额外的栈空间保存参数
    stack_size += (max_call_args - 8) * 4;
    locals_add_offset((max_call_args - 8) * 4);
  }
  printf("stack_size: %zu, local count: %d\n", stack_size, locals.count);
  // 对齐到 16 字节
  stack_size = (stack_size + 15) & ~15;

  outputf("%s:\n", func->name + 1); // + 1 是为了跳过函数名前的 @
  if (stack_size <= 2048) {
    outputf("  addi sp, sp, -%zu\n", stack_size);
  } else {
    outputf("  li t0, -%zu\n", stack_size);
    outputf("  add sp, sp, t0\n");
  }
  if (has_call) {
    // 保存 ra 寄存器
    outputf("  sw ra, %zu(sp)\n", stack_size - 4);
  }
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
      if (value->used_by.len == 0) {
        // 在处理 used_by 的时候，会将这个 value 也处理
        // 这里做个判断，避免重复处理
        // 比如说这两条 IR 指令:
        // %1 = add 1, 2
        // ret %1
        // basic block 里面会有 %1 = add 1, 2 和 ret %1 两条指令
        // ret 指令里面也会指向 %1 = add 1, 2
        // 如果不做判断，会导致 %1 = add 1, 2 被处理两次
        visit_koopa_raw_value(ptr);
      }
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
  visit_koopa_raw_slice(program.values); // 全局变量
  visit_koopa_raw_slice(program.funcs);  // 函数
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