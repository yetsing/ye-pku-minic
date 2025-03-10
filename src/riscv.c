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

static int temp_index = 0; // 给临时变量计数用的
// 函数返回时，需要恢复 sp ，所以需要记录栈的大小
// 即生成 ret 指令时，需要将 sp 加上这个大小，恢复栈空间
static size_t stack_size = 0;
// 是否调用了其他函数，如果调用了，需要保存 ra
// 寄存器； ret 指令时，需要恢复 ra 寄存器
static bool has_call = false;

typedef enum {
  VariableType_int,
  VariableType_array,
  VariableType_pointer,
} VariableType;

typedef struct Variable Variable;
typedef struct Variable {
  const char *name;
  VariableType type;
  int offset;
  int size; // 变量大小
  Variable *next;
  int count; // 计数
} Variable;

static Variable globals = {NULL, VariableType_int, 0, 0, NULL, 0}; // 全局变量
static Variable locals = {NULL, VariableType_int, 0, 0, NULL, 0}; // 局部变量

static void new_global_variable(const char *name) {
  assert(name != NULL);
  Variable *var = (Variable *)malloc(sizeof(Variable));
  var->name = name;
  var->offset = 0;
  var->next = globals.next;
  var->type = VariableType_int;
  var->size = 4;
  globals.next = var;
  globals.count++;
}

static bool is_global(const char *name) {
  Variable *var = globals.next;
  while (var != NULL) {
    if (strcmp(var->name, name) == 0) {
      return true;
    }
    var = var->next;
  }
  return false;
}

static Variable *new_variable(const char *name) {
  assert(name != NULL);
  Variable *var = (Variable *)malloc(sizeof(Variable));
  var->name = name;
  var->offset =
      locals.next == NULL ? 0 : locals.next->offset + locals.next->size;
  var->next = locals.next;
  var->size = 4;
  var->type = VariableType_int;
  locals.next = var;
  locals.count++;
  return var;
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
  return var == NULL ? 0 : var->offset + var->size;
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
  assert(temp_index > 0);
  int n = (temp_index - 1) * 4 + last;
  // assert(n >= 0 && n < stack_size);
  return n;
}

static int get_type_size(const koopa_raw_type_t ty);
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
static void visit_koopa_raw_call(const koopa_raw_call_t call, bool has_return);
static void visit_koopa_raw_global_alloc(const koopa_raw_global_alloc_t alloc,
                                         const char *name);
static void
visit_koopa_raw_get_elem_ptr(const koopa_raw_get_elem_ptr_t get_elem_ptr);
static void visit_koopa_raw_get_ptr(const koopa_raw_get_ptr_t get_ptr);
static void visit_global_init(const koopa_raw_value_t init);

static void store_to_stack(const char *src_register, int offset,
                           const char *temp_register) {
  if (offset >= 2048) {
    outputf("  li %s, %d\n", temp_register, offset);
    outputf("  add %s, sp, %s\n", temp_register, temp_register);
    outputf("  sw %s, 0(%s)\n", src_register, temp_register);
  } else {
    outputf("  sw %s, %d(sp)\n", src_register, offset);
  }
}

static void load_from_stack(const char *dst_register, int offset,
                            const char *temp_register) {
  if (offset >= 2048) {
    outputf("  li %s, %d\n", temp_register, offset);
    outputf("  add %s, sp, %s\n", temp_register, temp_register);
    outputf("  lw %s, 0(%s)\n", dst_register, temp_register);
  } else {
    outputf("  lw %s, %d(sp)\n", dst_register, offset);
  }
}

static void visit_koopa_raw_return(const koopa_raw_return_t ret) {
  outputf("\n  # === return ===\n");
  if (ret.value) {
    if (ret.value->kind.tag == KOOPA_RVT_INTEGER) {
      // 如果返回值是常量，直接使用立即数
      outputf("  li a0, %d\n", ret.value->kind.data.integer.value);
    } else {
      visit_koopa_raw_value(ret.value);
      // 从栈中取出返回值
      load_from_stack("a0", get_temp_offset(), "t0");
    }
  }
  if (has_call) {
    // 如果调用了其他函数，需要恢复 ra 寄存器
    int offset = stack_size - 4;
    load_from_stack("ra", offset, "t0");
  }

  // 函数返回，恢复栈空间 epilogue
  if (stack_size >= 2048) {
    outputf("  li t0, %zu\n", stack_size);
    outputf("  add sp, sp, t0\n");
  } else {
    outputf("  addi sp, sp, %zu\n", stack_size);
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
    load_from_stack("t0", lhs_offset, "t0");
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
    load_from_stack("t1", rhs_offset, "t1");
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
    fatalf("visit_koopa_raw_binary unknown op: %d\n", binary.op);
  }
  temp_index++; // 存在返回值，所以栈指针需要移动
  // 将结果存储到栈上
  int offset = get_temp_offset();
  store_to_stack(result_register, offset, "t1");
  outputf("  # === binary %d end ===\n", binary.op);
}

static void visit_koopa_raw_load(const koopa_raw_load_t load) {
  if (load.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    outputf("  la t0, %s\n", load.src->name + 1);
    outputf("  lw t0, 0(t0)\n");
    temp_index++; // 存在返回值，所以栈指针需要移动
    store_to_stack("t0", get_temp_offset(), "t1");
  } else if (load.src->kind.tag == KOOPA_RVT_ALLOC) {
    int offset = get_offset(load.src->name);
    load_from_stack("t0", offset, "t0");
    temp_index++; // 存在返回值，所以栈指针需要移动
    store_to_stack("t0", get_temp_offset(), "t1");
  } else if (load.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
             load.src->kind.tag == KOOPA_RVT_GET_PTR) {
    visit_koopa_raw_value(load.src);
    // 将值加载到 t0
    load_from_stack("t0", get_temp_offset(), "t0");
    // t0 存的是地址，需要再取一次
    outputf("  lw t0, 0(t0)\n");
    temp_index++; // 存在返回值，所以栈指针需要移动
    store_to_stack("t0", get_temp_offset(), "t1");
  } else {
    fatalf("visit_koopa_raw_load unknown src kind: %d\n", load.src->kind.tag);
  }
}

static void visit_koopa_raw_store(const koopa_raw_store_t store) {
  if (store.dest->kind.tag == KOOPA_RVT_ALLOC ||
      store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    outputf("  # === store alloc ===\n");
    if (store.value->kind.tag == KOOPA_RVT_FUNC_ARG_REF) {
      int index = store.value->kind.data.func_arg_ref.index;
      if (index < 8) {
        // 参数在 a0 ~ a7 中
        int offset = get_offset(store.dest->name);
        char src_reg[3] = {'a', '0' + index, '\0'};
        store_to_stack(src_reg, offset, "t0");
      } else {
        // 参数在栈上
        int offset = (int)stack_size + (index - 8) * 4;
        load_from_stack("t0", offset, "t0");
        // 结果已经在 t0 中了，直接存储到栈上
        // outputf("  lw t0, %d(sp)\n", get_temp_offset());
        offset = get_offset(store.dest->name);
        store_to_stack("t0", offset, "t1");
      }
      outputf("  # === end store arg ref ===\n");
    } else {
      visit_koopa_raw_value(store.value);
      // 结果已经在 t0 中了，直接存储到栈上
      // outputf("  lw t0, %d(sp)\n", get_temp_offset());
      if (is_global(store.dest->name)) {
        outputf("  la t1, %s\n", store.dest->name + 1);
        outputf("  sw t0, 0(t1)\n");
      } else {
        int offset = get_offset(store.dest->name);
        store_to_stack("t0", offset, "t1");
      }
    }
  } else if (store.dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
    // 保存的值
    int val_offset = 0;
    if (store.value->kind.tag != KOOPA_RVT_INTEGER) {
      visit_koopa_raw_value(store.value);
      val_offset = get_temp_offset();
    }
    // 保存的地址
    visit_koopa_raw_value(store.dest);
    int dst_offset = get_temp_offset();
    outputf("  # === store get_elem_ptr ===\n");
    if (store.value->kind.tag == KOOPA_RVT_INTEGER) {
      // 如果保存的值是常量，直接使用立即数
      outputf("  li t0, %d\n", store.value->kind.data.integer.value);
    } else {
      // 从栈中取出值
      load_from_stack("t0", val_offset, "t0");
    }
    // 从栈中取出地址
    load_from_stack("t1", dst_offset, "t1");
    // 将值保存到对应地址中
    outputf("  sw t0, 0(t1)\n");
  } else if (store.dest->kind.tag == KOOPA_RVT_GET_PTR) {
    // 保存的值
    int val_offset = 0;
    if (store.value->kind.tag != KOOPA_RVT_INTEGER) {
      visit_koopa_raw_value(store.value);
      val_offset = get_temp_offset();
    }
    // 保存的地址
    visit_koopa_raw_value(store.dest);
    int dst_offset = get_temp_offset();
    outputf("  # === store get_ptr ===\n");
    if (store.value->kind.tag == KOOPA_RVT_INTEGER) {
      // 如果保存的值是常量，直接使用立即数
      outputf("  li t0, %d\n", store.value->kind.data.integer.value);
    } else {
      // 从栈中取出值到 t0
      load_from_stack("t0", val_offset, "t0");
    }
    // 从栈中取出地址到 t1
    load_from_stack("t1", dst_offset, "t1");
    // 将值保存到对应地址中
    outputf("  sw t0, 0(t1)\n");
  } else {
    fatalf("visit_koopa_raw_store unknown dest kind: %d\n",
           store.dest->kind.tag);
  }
  outputf("  # === end store ===\n");
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

static void visit_koopa_raw_call(const koopa_raw_call_t call, bool has_return) {
  outputf("\n  # === call ===\n");
  int *arg_offsets = (int *)malloc(call.args.len * sizeof(int));
  memset(arg_offsets, 0, call.args.len * sizeof(int));
  // 先对调用参数求值
  for (int i = 0; i < call.args.len; i++) {
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
      int offset = arg_offsets[i];
      char dst_reg[3] = {'a', '0' + i, '\0'};
      load_from_stack(dst_reg, offset, "t0");
    }
  }
  // 如果参数个数大于 8，需要额外的栈空间保存参数
  for (int i = 8; i < call.args.len; i++) {
    const koopa_raw_value_t arg = call.args.buffer[i];
    if (arg->kind.tag == KOOPA_RVT_INTEGER) {
      // 参数是常量，直接使用立即数
      outputf("  li t0, %d\n", arg->kind.data.integer.value);
    } else {
      int offset = arg_offsets[i];
      load_from_stack("t0", offset, "t0");
    }
    int offset = (i - 8) * 4;
    store_to_stack("t0", offset, "t1");
  }

  // 调用函数
  outputf("  call %s\n", call.callee->name + 1); // + 1 是为了跳过函数名前的 @
  if (has_return) {
    // 保存返回值
    temp_index++; // 存在返回值，所以栈指针需要移动
    store_to_stack("a0", get_temp_offset(), "t0");
    outputf("  mv t0, a0\n");
  }
  outputf("  # === call end ===\n");
}

static void visit_global_init(const koopa_raw_value_t init) {
  if (init->kind.tag == KOOPA_RVT_INTEGER) {
    outputf("  .word %d\n", init->kind.data.integer.value);
  } else if (init->kind.tag == KOOPA_RVT_AGGREGATE) {
    for (size_t i = 0; i < init->kind.data.aggregate.elems.len; i++) {
      const koopa_raw_value_t elem = init->kind.data.aggregate.elems.buffer[i];
      visit_global_init(elem);
    }
  } else if (init->kind.tag == KOOPA_RVT_ZERO_INIT) {
    int size = get_type_size(init->ty);
    for (size_t i = 0; i < (size / 4); i++) {
      outputf("  .zero 4\n");
    }
  } else {
    fatalf("viist_global_init unknown kind: %d\n", init->kind.tag);
  }
}

static void visit_koopa_raw_global_alloc(const koopa_raw_global_alloc_t alloc,
                                         const char *name) {
  new_global_variable(name);
  outputf("  .global %s\n", name + 1);
  outputf("%s:\n", name + 1);
  visit_global_init(alloc.init);
}

static int get_type_size(const koopa_raw_type_t ty) {
  if (ty->tag == KOOPA_RTT_INT32 || ty->tag == KOOPA_RTT_POINTER) {
    return 4;
  } else if (ty->tag == KOOPA_RTT_ARRAY) {
    return ty->data.array.len * get_type_size(ty->data.array.base);
  } else {
    fatalf("get_array_size unknown type: %d\n", ty->tag);
    return -1;
  }
}

static void visit_koopa_raw_get_elem_ptr(const koopa_raw_get_elem_ptr_t gep) {
  if (gep.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    // src type: *[t, len]
    // getelemptr = src + sizeof(t) * index
    int index_offset = -1;
    if (gep.index->kind.tag != KOOPA_RVT_INTEGER) {
      visit_koopa_raw_value(gep.index);
      index_offset = get_temp_offset();
    }
    outputf("\n  # === get_elem_ptr global_alloc ===\n");
    // 全局变量
    outputf("  la t0, %s\n", gep.src->name + 1);
    if (gep.index->kind.tag == KOOPA_RVT_INTEGER) {
      outputf("  li t1, %d\n", gep.index->kind.data.integer.value);
    } else {
      assert(index_offset > 0);
      load_from_stack("t1", index_offset, "t1");
    }
    // 计算 sizeof(t)
    int size = get_type_size(gep.src->ty->data.pointer.base->data.array.base);
    outputf("  li t2, %d\n", size);
    outputf("  mul t1, t1, t2\n");
    outputf("  add t0, t0, t1\n");
    temp_index++; // 存在返回值，所以栈指针需要移动
    int offset = get_temp_offset();
    store_to_stack("t0", offset, "t1");
  } else if (gep.src->kind.tag == KOOPA_RVT_ALLOC) {
    // src type: *[t, len]
    // getelemptr = src + sizeof(t) * index
    assert(gep.src->ty->tag == KOOPA_RTT_POINTER);
    assert(gep.src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY);
    int index_offset = -1;
    if (gep.index->kind.tag != KOOPA_RVT_INTEGER) {
      visit_koopa_raw_value(gep.index);
      index_offset = get_temp_offset();
    }
    outputf("\n  # === get_elem_ptr ===\n");
    // 局部变量
    // 加载变量地址到 t0
    int offset = get_offset(gep.src->name);
    if (offset <= 2048) {
      outputf("  addi t0, sp, %d\n", offset);
    } else {
      outputf("  li t0, %d\n", offset);
      outputf("  add t0, sp, t0\n");
    }
    // 加载索引到 t1
    if (gep.index->kind.tag == KOOPA_RVT_INTEGER) {
      outputf("  li t1, %d\n", gep.index->kind.data.integer.value);
    } else {
      assert(index_offset > 0);
      load_from_stack("t1", index_offset, "t1");
    }
    // 计算 sizeof(t)
    int size = get_type_size(gep.src->ty->data.pointer.base->data.array.base);
    outputf("  li t2, %d\n", size);
    outputf("  mul t1, t1, t2\n");
    outputf("  add t0, t0, t1\n");
    temp_index++; // 存在返回值，所以栈指针需要移动
    store_to_stack("t0", get_temp_offset(), "t1");
  } else if (gep.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
             gep.src->kind.tag == KOOPA_RVT_GET_PTR) {
    // src type: *[t, len]
    // getelemptr = src + sizeof(t) * index
    assert(gep.src->ty->tag == KOOPA_RTT_POINTER);
    assert(gep.src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY);
    visit_koopa_raw_value(gep.src);
    int ele_offset = get_temp_offset();
    int index_offset = -1;
    if (gep.index->kind.tag != KOOPA_RVT_INTEGER) {
      visit_koopa_raw_value(gep.index);
      index_offset = get_temp_offset();
    }
    outputf("\n  # === get_elem_ptr ===\n");
    // 加载地址到 t0
    load_from_stack("t0", ele_offset, "t0");
    // 加载索引到 t1
    if (gep.index->kind.tag == KOOPA_RVT_INTEGER) {
      outputf("  li t1, %d\n", gep.index->kind.data.integer.value);
    } else {
      assert(index_offset > 0);
      load_from_stack("t1", index_offset, "t1");
    }
    // 计算 sizeof(t)
    int size = get_type_size(gep.src->ty->data.pointer.base->data.array.base);
    outputf("  li t2, %d\n", size);
    outputf("  mul t1, t1, t2\n");
    outputf("  add t0, t0, t1\n");
    temp_index++; // 存在返回值，所以栈指针需要移动
    store_to_stack("t0", get_temp_offset(), "t1");
  } else {
    fatalf("visit_koopa_raw_get_elem_ptr unknown src kind: %d\n",
           gep.src->kind.tag);
  }
  outputf("  # === get_elem_ptr end ===\n");
}

static void visit_koopa_raw_get_ptr(const koopa_raw_get_ptr_t get_ptr) {
  // get_ptr = src + sizeof(t) * index
  int index_offset = -1;
  if (get_ptr.index->kind.tag != KOOPA_RVT_INTEGER) {
    visit_koopa_raw_value(get_ptr.index);
    index_offset = get_temp_offset();
  }
  visit_koopa_raw_value(get_ptr.src);
  int src_offset = get_temp_offset();
  outputf("\n  # === get_ptr ===\n");
  // 加载地址到 t0
  load_from_stack("t0", src_offset, "t0");
  // 加载索引到 t1
  if (get_ptr.index->kind.tag == KOOPA_RVT_INTEGER) {
    outputf("  li t1, %d\n", get_ptr.index->kind.data.integer.value);
  } else {
    assert(index_offset > 0);
    load_from_stack("t1", index_offset, "t1");
  }
  // 计算 sizeof(t)
  int size = get_type_size(get_ptr.src->ty->data.pointer.base);
  outputf("  li t2, %d\n", size);
  outputf("  mul t1, t1, t2\n");
  outputf("  add t0, t0, t1\n");
  temp_index++; // 存在返回值，所以栈指针需要移动
  store_to_stack("t0", get_temp_offset(), "t1");
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
  case KOOPA_RVT_BINARY:
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
    visit_koopa_raw_call(kind.data.call, value->used_by.len > 0);
    break;
  case KOOPA_RVT_GLOBAL_ALLOC:
    visit_koopa_raw_global_alloc(kind.data.global_alloc, value->name);
    break;
  case KOOPA_RVT_GET_ELEM_PTR:
    visit_koopa_raw_get_elem_ptr(kind.data.get_elem_ptr);
    break;
  case KOOPA_RVT_GET_PTR:
    visit_koopa_raw_get_ptr(kind.data.get_ptr);
    break;
  default:
    fatalf("visit_koopa_raw_value unknown kind: %d\n", kind.tag);
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
        if (value->kind.tag == KOOPA_RVT_ALLOC) {
          // 分配局部变量
          Variable *variable = new_variable(value->name);
          assert(value->ty->tag == KOOPA_RTT_POINTER);
          const struct koopa_raw_type_kind *pval = value->ty->data.pointer.base;
          switch (pval->tag) {
          case KOOPA_RTT_INT32:
            stack_size += 4;
            variable->size = 4;
            variable->type = VariableType_int;
            break;
          case KOOPA_RTT_ARRAY: {
            int size = get_type_size(pval);
            stack_size += size;
            variable->size = size;
            variable->type = VariableType_array;
            break;
          }
          case KOOPA_RTT_POINTER:
            stack_size += 4;
            variable->size = 4;
            variable->type = VariableType_pointer;
            break;
          default:
            fatalf("visit_koopa_raw_function alloc unknown type: %d\n",
                   value->ty->tag);
          }
        } else {
          stack_size += 4;
        }
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
  // 对齐到 16 字节
  stack_size = (stack_size + 15) & ~15;

  outputf("%s:\n", func->name + 1); // + 1 是为了跳过函数名前的 @
  if (stack_size >= 2048) {
    outputf("  li t0, -%zu\n", stack_size);
    outputf("  add sp, sp, t0\n");
  } else {
    outputf("  addi sp, sp, -%zu\n", stack_size);
  }
  if (has_call) {
    // 保存 ra 寄存器
    int offset = stack_size - 4;
    store_to_stack("ra", offset, "t0");
  }
  visit_koopa_raw_slice(func->bbs);
}

static void visit_koopa_raw_slice(const koopa_raw_slice_t slice) {
  for (size_t i = 0; i < slice.len; i++) {
    const void *ptr = slice.buffer[i];
    // 根据 slice 的 kind 决定将 ptr 视作何种元素
    switch (slice.kind) {
    case KOOPA_RSIK_FUNCTION: {
      koopa_raw_function_t func = ptr;
      if (func->bbs.len == 0) {
        // 如果函数没有基本块，说明只是函数声明，直接跳过
        continue;
      }
      // + 1 是为了跳过函数名前的 @
      outputf("  .globl %s\n", ((koopa_raw_function_t)ptr)->name + 1);
      visit_koopa_raw_function(ptr);
      break;
    }
    case KOOPA_RSIK_BASIC_BLOCK:
      visit_koopa_raw_basic_block(ptr);
      break;
    case KOOPA_RSIK_VALUE: {
      const koopa_raw_value_t value = ptr;
      if ((value->used_by.len == 0) ||
          (value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)) {
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
      fatalf("visit_koopa_raw_slice unknown kind: %d\n", slice.kind);
    }
  }
}

static void visit_koopa_raw_program(const koopa_raw_program_t program) {
  outputf("  .data\n");
  visit_koopa_raw_slice(program.values); // 全局变量
  outputf("  .text\n");
  visit_koopa_raw_slice(program.funcs); // 函数
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