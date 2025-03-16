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

// #region 辅助变量和函数

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

typedef struct {
  koopa_raw_value_t value;
  int depth;
} TempValue;

typedef struct {
  TempValue *values;
  size_t len;
  size_t cap;
  int base_offset;
  int max_depth;
  int depth;
} TempValueManager;

static TempValueManager tv_manager = {NULL, 0, 0, 0, 0, 0};

static void tv_manager_reinit(int base_offset) {
  tv_manager.len = 0;
  tv_manager.cap = 16;
  tv_manager.max_depth = 0;
  tv_manager.depth = 0;
  tv_manager.base_offset = base_offset;
  tv_manager.values = (TempValue *)realloc(tv_manager.values,
                                           tv_manager.cap * sizeof(TempValue));
}

static void tv_manager_push(koopa_raw_value_t value) {
  if (tv_manager.len >= tv_manager.cap) {
    tv_manager.cap *= 2;
    tv_manager.values = (TempValue *)realloc(
        tv_manager.values, tv_manager.cap * sizeof(TempValue));
  }
  int idx = tv_manager.len;
  int depth = tv_manager.depth;
  tv_manager.values[idx] = (TempValue){value, depth};
  tv_manager.max_depth = MAX(tv_manager.max_depth, depth);
  tv_manager.depth++;
  tv_manager.len++;
}

static void tv_manager_pop(int n) {
  tv_manager.depth -= n;
  assert(tv_manager.depth >= 0);
}

static int tv_manager_get_offset(koopa_raw_value_t value) {
  for (size_t i = 0; i < tv_manager.len; i++) {
    if (tv_manager.values[i].value == value) {
      return tv_manager.base_offset + tv_manager.values[i].depth * 4;
    }
  }
  return -1;
}

static int tv_manager_bget_offset(koopa_raw_value_t value) {
  int offset = tv_manager_get_offset(value);
  assert(offset >= 0);
  return offset;
}

static int tv_manager_get_max_depth() { return tv_manager.max_depth; }

static void store_to_stack(const char *src_register, int offset,
                           const char *temp_register) {
  if (offset < 0) {
    return;
  }
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

#define REGISTER_COUNT 13
static const char *registers[REGISTER_COUNT] = {
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
    // t0 t1 不分配，需要留一些寄存器用来操作
    "t2",
    "t3",
    "t4",
    "t5",
    "t6",
};

typedef struct {
  koopa_raw_value_t value;
  const char *reg;
} RegisterAllocation;

typedef struct {
  const char *available[REGISTER_COUNT];
  int available_count;
  RegisterAllocation alolcations[REGISTER_COUNT];
  int allocations_count;
} RegisterManager;

static RegisterManager register_manager;
void register_manager_init(void) {
  register_manager.available_count = REGISTER_COUNT;
  for (int i = 0; i < REGISTER_COUNT; i++) {
    register_manager.available[i] = (char *)registers[i];
  }
  register_manager.allocations_count = 0;
}

const char *register_manager_allocate(koopa_raw_value_t value) {
  if (register_manager.available_count == 0) {
    return "";
  }
  for (int i = 0; i < register_manager.allocations_count; i++) {
    if (register_manager.alolcations[i].value == value) {
      return register_manager.alolcations[i].reg;
    }
  }

  const char *reg =
      register_manager.available[register_manager.available_count - 1];
  register_manager.available_count--;
  register_manager.allocations_count++;
  register_manager.alolcations[register_manager.allocations_count - 1] =
      (RegisterAllocation){.value = value, .reg = reg};

  if (value->name != NULL) {
    outputf("      # register_manager_allocate %s, %s\n", value->name, reg);
  } else {
    outputf("    # register_manager_allocate %p, %s\n", value, reg);
  }
  return reg;
}

void register_manager_free(const char *reg) {

  // 从 allocations 中删除
  for (int i = 0; i < register_manager.allocations_count; i++) {
    RegisterAllocation allocation = register_manager.alolcations[i];
    if (strcmp(allocation.reg, reg) == 0) {
      koopa_raw_value_t value = allocation.value;
      if (value->name != NULL) {
        outputf("      # register_manager_free %s, %s\n", value->name, reg);
      } else {
        outputf("    # register_manager_free %p, %s\n", value, reg);
      }
      for (int j = i; j < register_manager.allocations_count - 1; j++) {
        register_manager.alolcations[j] = register_manager.alolcations[j + 1];
      }
      register_manager.allocations_count--;
      register_manager.available[register_manager.available_count] = reg;
      register_manager.available_count++;
      break;
    }
  }
}

// 保存分配寄存器的值到栈上，释放所有寄存器
void register_manager_flush(void) {
  outputf("    # register_manager_flush\n");
  for (int i = 0; i < register_manager.allocations_count; i++) {
    RegisterAllocation alloc = register_manager.alolcations[i];
    if (alloc.value->kind.tag == KOOPA_RVT_ALLOC) {
      int offset = get_offset(alloc.value->name);
      store_to_stack(alloc.reg, offset, "t0");
    } else if (alloc.value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
      outputf("  la t0, %s\n", alloc.value->name + 1);
      outputf("  sw %s, 0(t0)\n", alloc.reg);
    } else {
      int offset = tv_manager_bget_offset(alloc.value);
      store_to_stack(alloc.reg, offset, "t0");
    }
  }

  register_manager_init();
}

// 从内存中加载值到寄存器
//     如果值已经在寄存器中，直接返回所在寄存器
//     否则，从内存中加载值到寄存器
// 返回所在寄存器
const char *register_manager_load_value(koopa_raw_value_t value,
                                        const char *reg, const char *temp_reg) {
  for (int i = 0; i < register_manager.allocations_count; i++) {
    RegisterAllocation alloc = register_manager.alolcations[i];
    if (alloc.value == value) {
      return alloc.reg;
    }
  }
  // 从内存中加载值到寄存器
  if (value->kind.tag == KOOPA_RVT_INTEGER) {
    if (value->kind.data.integer.value == 0) {
      // 如果值是 0，直接使用 x0 即可
      return "x0";
    }
    outputf("  li %s, %d\n", reg, value->kind.data.integer.value);
  } else if (value->kind.tag == KOOPA_RVT_ALLOC) {
    outputf("      # register_manager_load_value\n");
    int offset = get_offset(value->name);
    load_from_stack(reg, offset, temp_reg);
  } else if (value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    outputf("      # register_manager_load_value\n");
    outputf("  la %s, %s\n", reg, value->name + 1);
    outputf("  lw %s, 0(%s)\n", reg, reg);
  } else {
    outputf("      # register_manager_load_value\n");
    int offset = tv_manager_bget_offset(value);
    load_from_stack(reg, offset, temp_reg);
  }
  return reg;
}

static int get_type_size(const koopa_raw_type_t ty);
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

// #endregion

// #region visit IR 生成代码
static void visit_koopa_raw_return(const koopa_raw_return_t ret);
static void visit_koopa_raw_integer(const koopa_raw_integer_t n);
static void visit_koopa_raw_value(const koopa_raw_value_t value);
static void visit_koopa_raw_basic_block(const koopa_raw_basic_block_t block);
static void visit_koopa_raw_function(const koopa_raw_function_t func);
static void visit_koopa_raw_slice(const koopa_raw_slice_t slice);
static void visit_koopa_raw_program(const koopa_raw_program_t program);
static void visit_koopa_raw_binary(const koopa_raw_binary_t binary,
                                   int tv_offset,
                                   const koopa_raw_value_t value);
static void visit_koopa_raw_branch(const koopa_raw_branch_t branch);
static void visit_koopa_raw_jump(const koopa_raw_jump_t jump);
static void visit_koopa_raw_call(const koopa_raw_call_t call, int tv_offset);
static void visit_koopa_raw_global_alloc(const koopa_raw_global_alloc_t alloc,
                                         const char *name);
static void
visit_koopa_raw_get_elem_ptr(const koopa_raw_get_elem_ptr_t get_elem_ptr,
                             int tv_offset);
static void visit_koopa_raw_get_ptr(const koopa_raw_get_ptr_t get_ptr,
                                    int tv_offset);
static void visit_global_init(const koopa_raw_value_t init);

static void visit_koopa_raw_return(const koopa_raw_return_t ret) {
  outputf("    # return\n");
  if (ret.value) {
    const char *reg = register_manager_load_value(ret.value, "t0", "t0");
    outputf("  mv a0, %s\n", reg);
    register_manager_free(reg);
  }
  // 返回之前，需要将所有分配的寄存器的值保存到栈上
  register_manager_flush();
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
  assert(false);
  outputf("    # integer\n");
  outputf("  li t0, %d\n", n.value);
}

static void visit_koopa_raw_binary(const koopa_raw_binary_t binary,
                                   int tv_offset,
                                   const koopa_raw_value_t value) {
  outputf("    # binary %d\n", binary.op);
  const char *lhs_register =
      register_manager_load_value(binary.lhs, "t0", "t0");
  const char *rhs_register =
      register_manager_load_value(binary.rhs, "t1", "t1");

  register_manager_free(lhs_register);
  register_manager_free(rhs_register);

  // 目前所有变量都存储在栈上，所以这里可以直接使用 t0 作为结果寄存器
  bool no_register = false;
  const char *result_register = register_manager_allocate(value);
  if (strcmp(result_register, "") == 0) {
    result_register = "t0";
    no_register = true;
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
    fatalf("visit_koopa_raw_binary unknown op: %d\n", binary.op);
  }
  // 将结果存储到栈上
  if (tv_offset >= 0 && no_register) {
    store_to_stack(result_register, tv_offset, "t1");
  }
}

static void visit_koopa_raw_load(const koopa_raw_load_t load, int tv_offset,
                                 const koopa_raw_value_t value) {
  if (load.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    outputf("    # load %s\n", load.src->name);
    const char *src_reg = register_manager_load_value(load.src, "t0", "t0");
    const char *dest_reg = register_manager_allocate(value);
    if (strcmp(dest_reg, "") != 0) {
      outputf("  mv %s, %s\n", dest_reg, src_reg);
    } else {
      store_to_stack(src_reg, tv_offset, "t1");
    }
  } else if (load.src->kind.tag == KOOPA_RVT_ALLOC) {
    outputf("    # load %s\n", load.src->name);
    const char *src_reg = register_manager_load_value(load.src, "t0", "t0");
    const char *dest_reg = register_manager_allocate(value);
    if (strcmp(dest_reg, "") != 0) {
      outputf("  mv %s, %s\n", dest_reg, src_reg);
    } else {
      store_to_stack(src_reg, tv_offset, "t1");
    }
  } else if (load.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
             load.src->kind.tag == KOOPA_RVT_GET_PTR) {
    outputf("    # load %%xxx\n");
    // 加载值
    const char *src_reg = register_manager_load_value(load.src, "t0", "t0");
    register_manager_free(src_reg);
    // 存的是地址，需要再取一次
    outputf("  lw t0, 0(%s)\n", src_reg);
    const char *dest_reg = register_manager_allocate(value);
    if (strcmp(dest_reg, "") != 0) {
      outputf("  mv %s, t0\n", dest_reg);
    } else {
      store_to_stack("t0", tv_offset, "t1");
    }
  } else {
    fatalf("visit_koopa_raw_load unknown src kind: %d\n", load.src->kind.tag);
  }
}

static void visit_koopa_raw_store(const koopa_raw_store_t store) {
  outputf("    # store\n");
  if (store.dest->kind.tag == KOOPA_RVT_ALLOC ||
      store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    char src_reg[3] = {'t', '0', '\0'};
    if (store.value->kind.tag == KOOPA_RVT_FUNC_ARG_REF) {
      int index = store.value->kind.data.func_arg_ref.index;
      if (index < 8) {
        // 参数在 a0 ~ a7 中
        src_reg[0] = 'a';
        src_reg[1] = '0' + index;
      } else {
        // 参数在栈上
        src_reg[0] = 't';
        src_reg[1] = '0';
        int offset = (int)stack_size + (index - 8) * 4;
        load_from_stack(src_reg, offset, src_reg);
      }
      if (store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
        outputf("  la t1, %s\n", store.dest->name + 1);
        outputf("  sw %s, 0(t1)\n", src_reg);
      } else {
        int offset = get_offset(store.dest->name);
        store_to_stack(src_reg, offset, "t1");
      }
    } else {
      const char *reg = register_manager_load_value(store.value, src_reg, "t0");
      register_manager_free(reg);
      strncpy(src_reg, reg, 3);
      const char *dest_reg = register_manager_allocate(store.dest);
      if (strcmp(dest_reg, "") != 0) {
        outputf("  mv %s, %s\n", dest_reg, src_reg);
      } else {
        if (store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
          outputf("  la t1, %s\n", store.dest->name + 1);
          outputf("  sw %s, 0(t1)\n", src_reg);
        } else {
          int offset = get_offset(store.dest->name);
          store_to_stack(src_reg, offset, "t1");
        }
      }
    }

  } else if (store.dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
             store.dest->kind.tag == KOOPA_RVT_GET_PTR) {
    const char *value_reg =
        register_manager_load_value(store.value, "t0", "t0");
    const char *dest_addr_reg =
        register_manager_load_value(store.dest, "t1", "t1");
    register_manager_free(value_reg);
    register_manager_free(dest_addr_reg);

    // 将值保存到对应地址中
    outputf("  sw %s, 0(%s)\n", value_reg, dest_addr_reg);
  } else {
    fatalf("visit_koopa_raw_store unknown dest kind: %d\n",
           store.dest->kind.tag);
  }
  outputf("\n");
}

static void visit_koopa_raw_branch(const koopa_raw_branch_t branch) {
  outputf("    # br xx, %s, %s\n", branch.true_bb->name, branch.false_bb->name);
  const char *cond_register =
      register_manager_load_value(branch.cond, "t0", "t0");
  register_manager_free(cond_register);
  // 跳转之前，需要将所有分配的寄存器的值保存到栈上
  register_manager_flush();

  // +1 是为了跳过基本块名前的 %
  outputf("  bnez %s, %s\n", cond_register, branch.true_bb->name + 1);
  outputf("  j %s\n", branch.false_bb->name + 1);
}

static void visit_koopa_raw_jump(const koopa_raw_jump_t jump) {
  // 跳转之前，需要将所有分配的寄存器的值保存到栈上
  register_manager_flush();
  outputf("    # jump %s\n", jump.target->name);
  outputf("  j %s\n", jump.target->name + 1);
}

static void visit_koopa_raw_call(const koopa_raw_call_t call, int tv_offset) {
  // 调用之前，需要将所有分配的寄存器的值保存到栈上
  // TODO 优化：恢复分配的寄存器
  register_manager_flush();
  outputf("    # call %s\n", call.callee->name);
  // 参数传递
  for (int i = 0; i < 8 && i < call.args.len; i++) {
    const koopa_raw_value_t arg = call.args.buffer[i];
    if (arg->kind.tag == KOOPA_RVT_INTEGER) {
      // 参数是常量，直接使用立即数
      outputf("  li a%d, %d\n", i, arg->kind.data.integer.value);
    } else {
      int offset = tv_manager_bget_offset(arg);
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
      int offset = tv_manager_bget_offset(arg);
      load_from_stack("t0", offset, "t0");
    }
    int offset = (i - 8) * 4;
    store_to_stack("t0", offset, "t1");
  }

  // 调用函数
  outputf("  call %s\n", call.callee->name + 1); // + 1 是为了跳过函数名前的 @
  if (tv_offset >= 0) {
    // 保存返回值
    store_to_stack("a0", tv_offset, "t0");
    // outputf("  mv t0, a0\n");
  }
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
  outputf("\n");
  outputf("  .global %s\n", name + 1);
  outputf("%s:\n", name + 1);
  visit_global_init(alloc.init);
}

static void visit_koopa_raw_get_elem_ptr(const koopa_raw_get_elem_ptr_t gep,
                                         int tv_offset) {
  outputf("    # get_elem_ptr\n");
  if (gep.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
    // src type: *[t, len]
    // getelemptr = src + sizeof(t) * index
    // 全局变量
    const char *index_reg = register_manager_load_value(gep.index, "t0", "t0");
    register_manager_free(index_reg);
    // 计算 sizeof(t) * index
    int size = get_type_size(gep.src->ty->data.pointer.base->data.array.base);
    outputf("  li t1, %d\n", size);
    outputf("  mul t1, %s, t1\n", index_reg);
    // 加载全局变量地址到 t0
    outputf("  la t0, %s\n", gep.src->name + 1);
    outputf("  add t0, t0, t1\n");
    store_to_stack("t0", tv_offset, "t1");
  } else if (gep.src->kind.tag == KOOPA_RVT_ALLOC) {
    // src type: *[t, len]
    // getelemptr = src + sizeof(t) * index
    assert(gep.src->ty->tag == KOOPA_RTT_POINTER);
    assert(gep.src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY);
    // 局部变量
    // 加载索引
    const char *index_reg = register_manager_load_value(gep.index, "t0", "t0");
    register_manager_free(index_reg);
    // 计算 sizeof(t) * index
    int size = get_type_size(gep.src->ty->data.pointer.base->data.array.base);
    outputf("  li t1, %d\n", size);
    outputf("  mul t1, %s, t1\n", index_reg);
    // 加载变量地址到 t0
    int offset = get_offset(gep.src->name);
    if (offset <= 2048) {
      outputf("  addi t0, sp, %d\n", offset);
    } else {
      outputf("  li t0, %d\n", offset);
      outputf("  add t0, sp, t0\n");
    }
    outputf("  add t0, t0, t1\n");
    store_to_stack("t0", tv_offset, "t1");
  } else if (gep.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
             gep.src->kind.tag == KOOPA_RVT_GET_PTR) {
    // src type: *[t, len] or *t
    // getelemptr = src + sizeof(t) * index
    assert(gep.src->ty->tag == KOOPA_RTT_POINTER);
    assert(gep.src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY);
    // 加载索引到
    const char *index_reg = register_manager_load_value(gep.index, "t0", "t0");
    register_manager_free(index_reg);

    // 计算 sizeof(t) * index
    int size = get_type_size(gep.src->ty->data.pointer.base->data.array.base);
    outputf("  li t1, %d\n", size);
    outputf("  mul t1, %s, t1\n", index_reg);

    // 加载地址到
    const char *addr_reg = register_manager_load_value(gep.src, "t0", "t0");
    register_manager_free(addr_reg);

    outputf("  add t0, %s, t1\n", addr_reg);
    store_to_stack("t0", tv_offset, "t1");
  } else {
    fatalf("visit_koopa_raw_get_elem_ptr unknown src kind: %d\n",
           gep.src->kind.tag);
  }
}

static void visit_koopa_raw_get_ptr(const koopa_raw_get_ptr_t get_ptr,
                                    int tv_offset) {
  // get_ptr = src + sizeof(t) * index
  outputf("    # get_ptr\n");
  // 加载索引
  const char *index_reg =
      register_manager_load_value(get_ptr.index, "t0", "t0");
  register_manager_free(index_reg);
  // 计算 sizeof(t)
  int size = get_type_size(get_ptr.src->ty->data.pointer.base);
  outputf("  li t1, %d\n", size);
  outputf("  mul t1, %s, t1\n", index_reg);
  // 加载地址
  const char *addr_reg = register_manager_load_value(get_ptr.src, "t0", "t0");
  register_manager_free(addr_reg);
  outputf("  add t0, %s, t1\n", addr_reg);
  store_to_stack("t0", tv_offset, "t1");
}

static void visit_koopa_raw_value(const koopa_raw_value_t value) {
  koopa_raw_value_kind_t kind = value->kind;
  int tv_offset = tv_manager_get_offset(value);
  switch (kind.tag) {
  case KOOPA_RVT_RETURN:
    visit_koopa_raw_return(kind.data.ret);
    break;
  case KOOPA_RVT_INTEGER:
    visit_koopa_raw_integer(kind.data.integer);
    break;
  case KOOPA_RVT_BINARY:
    visit_koopa_raw_binary(kind.data.binary, tv_offset, value);
    break;
  case KOOPA_RVT_LOAD:
    visit_koopa_raw_load(kind.data.load, tv_offset, value);
    break;
  case KOOPA_RVT_STORE:
    visit_koopa_raw_store(kind.data.store);
    break;
  case KOOPA_RVT_ALLOC:
    // 分配局部变量
    // nothing to do
    outputf("    # alloc %s\n", value->name);
    break;
  case KOOPA_RVT_BRANCH:
    visit_koopa_raw_branch(kind.data.branch);
    break;
  case KOOPA_RVT_JUMP:
    visit_koopa_raw_jump(kind.data.jump);
    break;
  case KOOPA_RVT_CALL:
    visit_koopa_raw_call(kind.data.call, tv_offset);
    break;
  case KOOPA_RVT_GLOBAL_ALLOC:
    visit_koopa_raw_global_alloc(kind.data.global_alloc, value->name);
    break;
  case KOOPA_RVT_GET_ELEM_PTR:
    visit_koopa_raw_get_elem_ptr(kind.data.get_elem_ptr, tv_offset);
    break;
  case KOOPA_RVT_GET_PTR:
    visit_koopa_raw_get_ptr(kind.data.get_ptr, tv_offset);
    break;
  default:
    fatalf("visit_koopa_raw_value unknown kind: %d\n", kind.tag);
  }
}

static void visit_koopa_raw_basic_block(const koopa_raw_basic_block_t block) {
  // %entry 前已经输出了函数名，所以这里不需要输出 %entry 这个基本块名
  if (strcmp(block->name, "%entry") != 0) {
    outputf("\n%s:\n", block->name + 1); // + 1 是为了跳过基本块名前的 %
  }
  visit_koopa_raw_slice(block->insts);
}

static bool is_temp_value(const koopa_raw_value_t value) {
  return value && value->kind.tag != KOOPA_RVT_INTEGER &&
         value->kind.tag != KOOPA_RVT_ZERO_INIT &&
         value->kind.tag != KOOPA_RVT_UNDEF &&
         value->kind.tag != KOOPA_RVT_AGGREGATE &&
         value->kind.tag != KOOPA_RVT_FUNC_ARG_REF &&
         value->kind.tag != KOOPA_RVT_BLOCK_ARG_REF &&
         value->kind.tag != KOOPA_RVT_ALLOC &&
         value->kind.tag != KOOPA_RVT_GLOBAL_ALLOC &&
         value->ty->tag != KOOPA_RTT_UNIT;
}

static void handle_tv_stack(const koopa_raw_value_t value) {
  switch (value->kind.tag) {
  case KOOPA_RVT_INTEGER:
  case KOOPA_RVT_ZERO_INIT:
  case KOOPA_RVT_UNDEF:
  case KOOPA_RVT_AGGREGATE:
  case KOOPA_RVT_FUNC_ARG_REF:
  case KOOPA_RVT_BLOCK_ARG_REF:
  case KOOPA_RVT_ALLOC:
    // These types don't need special handling
    break;

  case KOOPA_RVT_GLOBAL_ALLOC:
    // Global allocations are handled separately
    break;

  case KOOPA_RVT_LOAD: {
    koopa_raw_value_t src = value->kind.data.load.src;
    if (is_temp_value(src)) {
      tv_manager_pop(1);
    }
    break;
  }

  case KOOPA_RVT_STORE: {
    koopa_raw_value_t dest = value->kind.data.store.dest;
    koopa_raw_value_t src = value->kind.data.store.value;
    if (is_temp_value(src)) {
      tv_manager_pop(1);
    }
    if (is_temp_value(dest)) {
      tv_manager_pop(1);
    }
    break;
  }

  case KOOPA_RVT_GET_PTR: {
    koopa_raw_value_t src = value->kind.data.get_ptr.src;
    if (is_temp_value(src)) {
      tv_manager_pop(1);
    }
    break;
  }

  case KOOPA_RVT_GET_ELEM_PTR: {
    koopa_raw_value_t src = value->kind.data.get_elem_ptr.src;
    if (is_temp_value(src)) {
      tv_manager_pop(1);
    }
    break;
  }

  case KOOPA_RVT_BINARY: {
    koopa_raw_value_t lhs = value->kind.data.binary.lhs;
    koopa_raw_value_t rhs = value->kind.data.binary.rhs;
    if (is_temp_value(lhs)) {
      tv_manager_pop(1);
    }
    if (is_temp_value(rhs)) {
      tv_manager_pop(1);
    }
    break;
  }

  case KOOPA_RVT_BRANCH: {
    koopa_raw_value_t cond = value->kind.data.branch.cond;
    if (is_temp_value(cond)) {
      tv_manager_pop(1);
    }
    break;
  }

  case KOOPA_RVT_JUMP:
    break;

  case KOOPA_RVT_CALL: {
    koopa_raw_call_t call = value->kind.data.call;
    for (size_t i = 0; i < call.args.len; i++) {
      koopa_raw_value_t arg = call.args.buffer[i];
      if (is_temp_value(arg)) {
        tv_manager_pop(1);
      }
    }
    break;
  }

  case KOOPA_RVT_RETURN: {
    koopa_raw_value_t ret = value->kind.data.ret.value;
    if (is_temp_value(ret)) {
      tv_manager_pop(1);
    }
    break;
  }

  default:
    fatalf("handle_tv_stack: unknown kind: %d\n", value->kind.tag);
  }
}

// 给临时变量分配栈空间
__attribute__((unused)) static void
assign_stack_of_temp_value(const koopa_raw_slice_t slice, int base_offset) {
  /*
    由于临时变量都是用完就丢，所以我们可以用表达式栈来确定临时变量的栈偏移，这样可以减少栈空间的使用
    比如
      %3 = add %1, %2

      [%1]     先将 %1 压入栈
      [%1, %2] 再将 %2 压入栈，现在栈里面有 %1 %2 两个值
      [%3]     将 %1 %2 出栈，计算 %1 + %2 的值，将结果压入栈
               %1 %2 都不会再使用，所以可以复用栈空间

    这种情况我们只需要两个临时变量的栈空间，而不需要三个
  */
  tv_manager_reinit(base_offset);
  for (size_t i = 0; i < slice.len; i++) {
    const koopa_raw_basic_block_t block = slice.buffer[i];
    for (size_t j = 0; j < block->insts.len; j++) {
      const koopa_raw_value_t value = block->insts.buffer[j];
      handle_tv_stack(value);
      if (value->ty->tag != KOOPA_RTT_UNIT &&
          value->kind.tag != KOOPA_RVT_ALLOC) {
        tv_manager_push(value);
      }
    }
  }
}

static void visit_koopa_raw_function(const koopa_raw_function_t func) {
  locals_reset();
  /**
    栈帧变量分配情况，从低到高依次为：
      调用函数参数
      局部变量
      临时变量（IR 中 % 开头的）
      ra 寄存器
    例如下面这样：
      sp + 12: %2
      sp +  8: %1
      sp +  4: %0
      sp +  0: @x
  */
  // 计算函数需要的栈空间
  stack_size = 0;
  has_call = false;
  int max_call_args = 0;
  int temp_base_offset = 0; // 临时变量的基地址
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
            temp_base_offset += variable->size;
            break;
          case KOOPA_RTT_ARRAY: {
            int size = get_type_size(pval);
            stack_size += size;
            variable->size = size;
            variable->type = VariableType_array;
            temp_base_offset += variable->size;
            break;
          }
          case KOOPA_RTT_POINTER:
            stack_size += 4;
            variable->size = 4;
            variable->type = VariableType_pointer;
            temp_base_offset += variable->size;
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
    // 需要额外的栈空间保存 t0-6 a0-7 寄存器
    stack_size += REGISTER_COUNT * 4;
  }
  if (max_call_args > 8) {
    // 如果函数调用的参数个数大于 8，需要额外的栈空间保存参数
    int size = (max_call_args - 8) * 4;
    stack_size += size;
    temp_base_offset += size;
    locals_add_offset(size);
  }
  assign_stack_of_temp_value(func->bbs, temp_base_offset);
  stack_size += (tv_manager_get_max_depth() * 4);
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

  for (size_t i = 0; i < func->bbs.len; i++) {
    // 实现基本块的寄存器分配
    register_manager_init();
    assert(func->bbs.kind == KOOPA_RSIK_BASIC_BLOCK);
    const koopa_raw_basic_block_t block = func->bbs.buffer[i];
    visit_koopa_raw_basic_block(block);
  }
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
      outputf("\n");
      outputf("  .global %s\n", ((koopa_raw_function_t)ptr)->name + 1);
      visit_koopa_raw_function(ptr);
      break;
    }
    case KOOPA_RSIK_BASIC_BLOCK:
      visit_koopa_raw_basic_block(ptr);
      break;
    case KOOPA_RSIK_VALUE: {
      visit_koopa_raw_value(ptr);
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

// #endregion

void riscv_perf_codegen(const char *ir, const char *output_file) {
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