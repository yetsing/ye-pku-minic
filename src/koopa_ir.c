#include "koopa_ir.h"

#include <assert.h>
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
// while 计数，用来生成唯一的标签
int while_index = 0;
// while body 计数，用来给 break continue 生成唯一标签
int while_body_index = 0;
// while 栈结构，用来辅助 break continue 生成
IntStack while_stack;
// 逻辑运算（ && || ）计数，用来生成唯一的标签
int logic_index = 0;
bool output_ret_inst = false;
// 当前正在处理的函数
AstFuncDef *current_func_def = NULL;

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

typedef enum {
  SymbolType_int,
  SymbolType_func,
  SymbolType_array,
} SymbolType;

typedef struct {
  BType return_type;
  BType *param_types;
  int param_count;
} FunctionType;

void update_func_type(AstFuncDef *func_def, FunctionType *func_type) {
  func_type->return_type = func_def->func_type;
  func_type->param_count = func_def->param_count;
  func_type->param_types = malloc(sizeof(BType) * func_type->param_count);
  FuncParam *param = func_def->params;
  for (int i = 0; i < func_type->param_count; i++) {
    func_type->param_types[i] = param->type;
    param = param->next;
  }
}

typedef struct Symbol Symbol;
typedef struct Symbol {
  const char *name;
  bool is_const_value;
  int value;
  int level;    // 符号的作用域
  int index;    // 符号的计数，用来生成唯一的符号名
  Symbol *next; // 指向下一个符号

  SymbolType type;
  FunctionType func_type;
  int *dimensions; // 数组的维度
  int dimension_count;
} Symbol;

static Symbol symbol_table_head = {NULL, false, 0, 0, 0, NULL, SymbolType_int};

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

static Symbol *new_symbol(const char *name, SymbolType type) {
  Symbol *found = find_symbol(name);
  if (found != NULL && found->level == symbol_table_head.level) {
    fprintf(stderr, "符号 %s 已经存在\n", name);
    exit(1);
  }
  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->name = name;
  symbol->is_const_value = false;
  symbol->value = 0;
  symbol->level = symbol_table_head.level;
  symbol->next = symbol_table_head.next;
  symbol->index = symbol_table_head.index++;
  symbol->type = type;
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

int eval_const_exp(AstExp *exp);
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

int eval_array_value(AstArrayValue *array_value) {
  AstExp **new_elements = malloc(sizeof(AstExp *) * array_value->count);
  for (int i = 0; i < array_value->count; i++) {
    if (array_value->elements[i]->type == AST_ARRAY_VALUE) {
      eval_const_exp(array_value->elements[i]);
      new_elements[i] = array_value->elements[i];
    } else {
      int val = eval_const_exp(array_value->elements[i]);
      AstNumber *number = new_ast_number();
      number->number = val;
      new_elements[i] = (AstExp *)number;
    }
  }
  array_value->elements = new_elements;
  return -1;
}

int eval_const_exp(AstExp *exp) {
  switch (exp->type) {
  case AST_NUMBER:
    return ((AstNumber *)exp)->number;
  case AST_ARRAY_VALUE:
    return eval_array_value((AstArrayValue *)exp);
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
  case AST_ARRAY_VALUE: {
    AstArrayValue *array_value = (AstArrayValue *)exp;
    for (int i = 0; i < array_value->count; i++) {
      array_value->elements[i] = optimize_exp(array_value->elements[i]);
    }
    return exp;
  }
  case AST_ARRAY_ACCESS: {
    AstArrayAccess *array_access = (AstArrayAccess *)exp;
    for (int i = 0; i < array_access->indexes.count; i++) {
      array_access->indexes.elements[i] =
          optimize_exp(array_access->indexes.elements[i]);
    }
    return exp;
  }
  case AST_FUNC_CALL: {
    AstFuncCall *func_call = (AstFuncCall *)exp;
    for (int i = 0; i < func_call->count; i++) {
      func_call->args[i] = optimize_exp(func_call->args[i]);
    }
    return exp;
  }
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
  case AST_NUMBER: {
    return exp;
  }
  default:
    fatalf("未知的表达式类型 %s\n", ast_type_to_string(exp->type));
    return NULL;
  }
}

void print_int_array(const char *prefix, int coordinates[], int n) {
  printf("%s: (", prefix);
  for (int i = 0; i < n; i++) {
    printf("%d, ", coordinates[i]);
  }
  printf(")\n");
}

/**
 * 将多维数组展开为一维数组
 * @param dimensions 数组的维度
 * @param count 维度的数量
 * @param val 数组的值
 * @param coordinates 当前的坐标
 * @param current 当前的维度
 * @param result 一维数组

*/
void do_flatten(int dimensions[], int count, AstArrayValue *val,
                int coordinates[], int current, ExpArray *result) {
  assert(current > 0);
  // printf("%*sdo_flatten current: %d\n", 2 * (count - current), " ", current);
  for (int i = 0; i < val->count; i++) {
    // print_int_array("coordinates", coordinates, count);
    if (val->elements[i]->type == AST_ARRAY_VALUE) {
      assert(coordinates[count - 1] == 0);
      int origin = coordinates[count - current];
      do_flatten(dimensions, count, (AstArrayValue *)val->elements[i],
                 coordinates, current - 1, result);
      // 碰到数组，对应维度进一，低维度需要全部置为 0
      // 这里不直接加 1 的原因：
      //    如果数组里面是全满的，就会发生进位，直接加 1 有问题；
      //    所以使用原始值加 1
      coordinates[count - current] = origin + 1;
      for (int i = count - current + 1; i < count; i++) {
        coordinates[i] = 0;
      }
    } else {
      // 碰到非数组，维度降到最低，开始填充
      current = 1;

      int index = 0;
      for (int i = 0; i < count; i++) {
        int n = coordinates[i];
        for (int j = i + 1; j < count; j++) {
          n *= dimensions[j];
        }
        index += n;
      }
      // printf("set index %d <", index);
      // val->elements[i]->dump((AstBase *)val->elements[i], 0);
      // printf(">\n");
      result->elements[index] = val->elements[i];
      coordinates[count - 1]++;

      // 坐标进位，同时更新下一个括号的维度
      for (int i = count - 1; i > 0; i--) {
        if (coordinates[i] == dimensions[i]) {
          coordinates[i] = 0;
          coordinates[i - 1]++;
          current = count - i + 1;
        } else {
          break;
        }
      }
      assert(coordinates[0] <= dimensions[0]); // 进位不能超过最高的维度
    }
    // print_int_array("  ", coordinates, count);
  }
}

// 比如声明是 int a[1][2][3] ，dimensions 就是 [1, 2, 3]
AstExp *flatten_multi_dimension_array(int dimensions[], int count,
                                      AstExp *val) {
  if (count == 1) {
    return val;
  }
  assert(val->type == AST_ARRAY_VALUE);
  // print_int_array("dimensions", dimensions, count);
  // printf("flatten_multi_dimension_array length: %d\n",
  //  ((AstArrayValue *)val)->count);
  int total_count = 1;
  for (int i = 0; i < count; i++) {
    total_count *= dimensions[i];
  }
  // printf("total_count: %d\n", total_count);
  ExpArray result;
  init_exp_array(&result);
  result.count = total_count;
  result.capacity = total_count;
  result.elements = malloc(sizeof(AstExp *) * result.capacity);
  for (int i = 0; i < total_count; i++) {
    AstNumber *number = new_ast_number();
    number->number = 0;
    result.elements[i] = (AstExp *)number;
  }

  int *coordinates = malloc(sizeof(int) * total_count);
  memset(coordinates, 0, sizeof(int) * total_count);
  do_flatten(dimensions, count, (AstArrayValue *)val, coordinates, count,
             &result);

  AstArrayValue *array_value = new_ast_array_value();
  array_value->count = total_count;
  array_value->capacity = total_count;
  array_value->elements = result.elements;
  return (AstExp *)array_value;
}

void optimize_const_decl(AstConstDecl *decl) {
  AstConstDef *def = decl->def;
  while (def) {
    assert(def->val != NULL);
    SymbolType symbol_type = SymbolType_int;
    if (def->dimensions.count > 0) {
      int dimensions[def->dimensions.count];
      for (int i = 0; i < def->dimensions.count; i++) {
        int n = eval_const_exp(def->dimensions.elements[i]);
        assert(n > 0);
        dimensions[i] = n;
        AstNumber *number = new_ast_number();
        number->number = n;
        def->dimensions.elements[i] = (AstExp *)number;
      }
      symbol_type = SymbolType_array;
      def->val = flatten_multi_dimension_array(dimensions,
                                               def->dimensions.count, def->val);
    }
    int value = eval_const_exp(def->val);
    Symbol *symbol = new_symbol(def->name, symbol_type);
    symbol->is_const_value = true;
    symbol->value = value;
    def = def->next;
  }
}

void optimize_var_decl(AstVarDecl *decl) {
  AstVarDef *def = decl->def;
  while (def) {
    if (def->dimensions.count > 0) {
      for (int i = 0; i < def->dimensions.count; i++) {
        int n = eval_const_exp(def->dimensions.elements[i]);
        assert(n > 0);
        AstNumber *number = new_ast_number();
        number->number = n;
        def->dimensions.elements[i] = (AstExp *)number;
      }
    }
    if (def->val) {
      def->val = optimize_exp(def->val);
      if (def->dimensions.count > 0) {
        int dimensions[def->dimensions.count];
        for (int i = 0; i < def->dimensions.count; i++) {
          dimensions[i] = ((AstNumber *)def->dimensions.elements[i])->number;
        }
        def->val = flatten_multi_dimension_array(
            dimensions, def->dimensions.count, def->val);
      }
    }
    def = def->next;
  }
}

void optimize_global_var_decl(AstVarDecl *decl) {
  AstVarDef *def = decl->def;
  while (def) {
    if (def->dimensions.count > 0) {
      int dimensions[def->dimensions.count];
      for (int i = 0; i < def->dimensions.count; i++) {
        int n = eval_const_exp(def->dimensions.elements[i]);
        assert(n > 0);
        AstNumber *number = new_ast_number();
        number->number = n;
        def->dimensions.elements[i] = (AstExp *)number;
        dimensions[i] = n;
      }
      if (def->val) {
        assert(def->val->type == AST_ARRAY_VALUE);
        eval_const_exp(def->val);
        def->val = flatten_multi_dimension_array(
            dimensions, def->dimensions.count, def->val);
      }
    } else {
      if (def->val) {
        int value = eval_const_exp(def->val);
        AstNumber *number = new_ast_number();
        number->number = value;
        def->val = (AstExp *)number;
      }
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
    assign_stmt->lhs = optimize_exp(assign_stmt->lhs);
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
    break;
  }
  case AST_WHILE_STMT: {
    AstWhileStmt *while_stmt = (AstWhileStmt *)stmt;
    while_stmt->condition = optimize_exp(while_stmt->condition);
    optimize_stmt(while_stmt->body);
    break;
  }
  case AST_BREAK_STMT:
  case AST_CONTINUE_STMT:
    break;

  default:
    fatalf("未知的语句类型 %s\n", ast_type_to_string(stmt->type));
  }
}

void optimize_block(AstBlock *block) {
  AstStmt *stmt = block->stmt;
  enter_scope();
  while (stmt) {
    switch (stmt->type) {
    case AST_CONST_DECL: {
      optimize_const_decl((AstConstDecl *)stmt);
      break;
    }
    case AST_VAR_DECL: {
      optimize_var_decl((AstVarDecl *)stmt);
      break;
    }
    case AST_EMPTY_STMT:
      // 移除空语句
      break;
    default:
      optimize_stmt(stmt);
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
}

// 优化 AST，工作包括：
//  - 移除一元加法表达式
//  - 数字计算，例如 1 + 2 -> 3
//  - 常量替换，例如 const a = 1; const b = a + 2; -> const a = 1; const b = 3;
void optimize_comp_unit(AstCompUnit *comp_unit) {
  for (int i = 0; i < comp_unit->count; i++) {
    AstBase *def = comp_unit->defs[i];
    switch (def->type) {

    case AST_FUNC_DEF: {
      AstFuncDef *func_def = (AstFuncDef *)def;
      optimize_block(func_def->block);
      break;
    }
    case AST_CONST_DECL: {
      optimize_const_decl((AstConstDecl *)def);
      break;
    }
    case AST_VAR_DECL: {
      optimize_global_var_decl((AstVarDecl *)def);
      break;
    }
    default: {
      fatalf("未知的定义类型\n");
    }
    }
  }
  // 重置符号表
  reset_symbol_table();

  // printf("    === 优化后的 AST ===\n");
  // comp_unit->base.dump((AstBase *)comp_unit, 4);
  // printf("\n");
}
// #endregion

// #region 生成 IR

static void codegen_exp(AstExp *exp);
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

static void codegen_binary_exp(AstBinaryExp *exp) {
  // 短路求值
  switch (exp->op) {
  case BinaryOpType_AND: {
    /*
      a && b 可以变成下面的语句
      int result = 0;
      if (a != 0) {
        result = b != 0;
      }
    */
    logic_index++;
    int current_logic_index = logic_index;
    // 引入中间变量 result
    // int result = 0
    outputf("  %%result_%d = alloc i32\n", current_logic_index);
    outputf("  store 0, %%result_%d\n", current_logic_index);
    codegen_exp(exp->lhs);
    char *lhs = exp_sign(exp->lhs);
    // if (a != 0) {
    //   result = b != 0;
    // }
    outputf("  br %s, %%and_true_%d, %%and_end_%d\n", lhs, current_logic_index,
            current_logic_index);
    outputf("%%and_true_%d:\n", current_logic_index);
    codegen_exp(exp->rhs);
    char *rhs = exp_sign(exp->rhs);
    outputf("  %%%d = ne %s, 0\n", temp_sign_index, rhs);
    outputf("  store %%%d, %%result_%d\n", temp_sign_index,
            current_logic_index);
    outputf("  jump %%and_end_%d\n", current_logic_index);
    temp_sign_index++;
    outputf("%%and_end_%d:\n", current_logic_index);
    // 按表达式的约定，把结果放到临时值
    outputf("  %%%d = load %%result_%d\n", temp_sign_index,
            current_logic_index);
    temp_sign_index++;
    free(lhs);
    free(rhs);
    return;
  }
  case BinaryOpType_OR: {
    /*
      a || b 可以变成下面的语句
      int result = 1;
      if (a == 0) {
        result = b != 0;
      }
    */
    logic_index++;
    int current_logic_index = logic_index;
    // 引入中间变量 result
    // int result = 1
    outputf("  %%result_%d = alloc i32\n", current_logic_index);
    outputf("  store 1, %%result_%d\n", current_logic_index);
    codegen_exp(exp->lhs);
    char *lhs = exp_sign(exp->lhs);
    // if (a == 0) {
    //   result = b != 0;
    // }
    outputf("  br %s, %%or_end_%d, %%or_false_%d\n", lhs, current_logic_index,
            current_logic_index);
    outputf("%%or_false_%d:\n", current_logic_index);
    codegen_exp(exp->rhs);
    char *rhs = exp_sign(exp->rhs);
    outputf("  %%%d = ne %s, 0\n", temp_sign_index, rhs);
    outputf("  store %%%d, %%result_%d\n", temp_sign_index,
            current_logic_index);
    outputf("  jump %%or_end_%d\n", current_logic_index);
    temp_sign_index++;
    outputf("%%or_end_%d:\n", current_logic_index);
    // 按表达式的约定，把结果放到临时值
    outputf("  %%%d = load %%result_%d\n", temp_sign_index,
            current_logic_index);
    temp_sign_index++;
    free(lhs);
    free(rhs);
    return;
  }
  default:
    break;
  }

  codegen_exp(exp->lhs);
  char *lhs = exp_sign(exp->lhs);
  codegen_exp(exp->rhs);
  char *rhs = exp_sign(exp->rhs);
  switch (exp->op) {
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
    fprintf(stderr, "未知的二元运算符 %c\n", exp->op);
    exit(1);
  }
  free(lhs);
  free(rhs);
}

static void codegen_func_call(AstFuncCall *func_call) {
  Symbol *symbol = find_symbol(func_call->ident->name);
  if (symbol == NULL) {
    fatalf("调用未定义的函数 %s\n", func_call->ident->name);
  }
  if (symbol->type != SymbolType_func) {
    fatalf("调用非函数符号 %s\n", func_call->ident->name);
  }
  if (symbol->func_type.param_count != func_call->count) {
    fatalf("调用函数 %s 参数个数不匹配\n", func_call->ident->name);
  }

  AstExp **args = func_call->args;
  char **signs = malloc(sizeof(char *) * func_call->count);
  for (int i = 0; i < func_call->count; i++) {
    codegen_exp(args[i]);
    signs[i] = exp_sign(args[i]);
  }
  if (symbol->func_type.return_type == BType_VOID) {
    outputf("  call @%s(", symbol->name);
  } else {
    outputf("  %%%d = call @%s(", temp_sign_index, symbol->name);
    temp_sign_index++;
  }
  for (int i = 0; i < func_call->count; i++) {
    outputf("%s%s", signs[i], (i == func_call->count - 1) ? "" : ", ");
  }
  outputf(")\n");

  for (int i = 0; i < func_call->count; i++) {
    free(signs[i]);
  }
  free(signs);
}

static void codegen_array_access(AstArrayAccess *array_access) {
  Symbol *symbol = find_symbol(array_access->name);
  if (symbol == NULL) {
    fatalf("访问未定义的数组变量 %s\n", array_access->name);
  }
  if (symbol->type != SymbolType_array) {
    fatalf("访问非数组变量 %s\n", array_access->name);
  }
  const char *name = symbol_unique_name(symbol);
  if (array_access->indexes.count == 1) {
    codegen_exp(array_access->indexes.elements[0]);
    outputf("  %%ptr_%d = getelemptr %s, %s\n", temp_sign_index, name,
            exp_sign(array_access->indexes.elements[0]));
    outputf("  %%%d = load %%ptr_%d\n", temp_sign_index, temp_sign_index);
    temp_sign_index++;
    return;
  }

  // 多维数组
  int *steps = malloc(sizeof(int) * symbol->dimension_count);
  steps[symbol->dimension_count - 1] = 1;
  for (int i = symbol->dimension_count - 2; i >= 0; i--) {
    steps[i] = steps[i + 1] * symbol->dimensions[i + 1];
  }
  int *indexes = malloc(sizeof(int) * array_access->indexes.count);
  for (int i = 0; i < array_access->indexes.count; i++) {
    codegen_exp(array_access->indexes.elements[i]);
    outputf("  %%%d = mul %s, %d\n", temp_sign_index,
            exp_sign(array_access->indexes.elements[i]),
            steps[i + symbol->dimension_count - array_access->indexes.count]);
    indexes[i] = temp_sign_index;
    temp_sign_index++;
  }
  // 累加所有维度的索引
  int result_index = indexes[0];
  for (int i = 1; i < array_access->indexes.count; i++) {
    outputf("  %%%d = add %%%d, %%%d\n", temp_sign_index, result_index,
            indexes[i]);
    result_index = temp_sign_index;
    temp_sign_index++;
  }
  outputf("  %%ptr_%d = getelemptr %s, %%%d\n", temp_sign_index, name,
          result_index);
  outputf("  %%%d = load %%ptr_%d\n", temp_sign_index, temp_sign_index);
  temp_sign_index++;
  return;
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
    codegen_binary_exp((AstBinaryExp *)exp);
    break;
  }

  case AST_NUMBER:
    // nothing to do
    break;
  case AST_IDENTIFIER:
    codegen_identifier((AstIdentifier *)exp);
    break;
  case AST_FUNC_CALL:
    codegen_func_call((AstFuncCall *)exp);
    break;
  case AST_ARRAY_ACCESS:
    codegen_array_access((AstArrayAccess *)exp);
    break;
  default:
    fprintf(stderr, "未知的表达式类型 %s\n", ast_type_to_string(exp->type));
    exit(1);
  }
}

static void codegen_return_stmt(AstReturnStmt *stmt) {
  if (stmt->exp) {
    if (current_func_def->func_type == BType_VOID) {
      fatalf("void 函数只能出现不带返回值的 return 语句\n");
    }
    codegen_exp(stmt->exp);
    outputf("  ret %s\n", exp_sign(stmt->exp));
  } else {
    if (current_func_def->func_type != BType_VOID) {
      warnf("非 void 函数没有 return 返回值\n");
    }
    outputf("  ret\n");
  }
}

static void codegen_assign_stmt(AstAssignStmt *stmt) {
  if (stmt->lhs->type == AST_IDENTIFIER) {
    AstIdentifier *ident = (AstIdentifier *)stmt->lhs;
    Symbol *symbol = find_symbol(ident->name);
    if (symbol == NULL) {
      fatalf("赋值未定义的符号 %s\n", ident->name);
    }
    codegen_exp(stmt->exp);
    const char *name = symbol_unique_name(symbol);
    outputf("  store %s, %s\n", exp_sign(stmt->exp), name);
    free((void *)name);
  } else if (stmt->lhs->type == AST_ARRAY_ACCESS) {
    codegen_exp(stmt->exp);
    const char *value_sign = exp_sign(stmt->exp);
    AstArrayAccess *array_access = (AstArrayAccess *)stmt->lhs;
    assert(array_access->indexes.count == 1);
    codegen_exp(array_access->indexes.elements[0]);
    const char *index_sign = exp_sign(array_access->indexes.elements[0]);
    Symbol *symbol = find_symbol(array_access->name);
    if (symbol == NULL) {
      fatalf("赋值未定义的数组变量 %s\n", array_access->name);
    }
    if (symbol->is_const_value) {
      fatalf("不能给常量赋值 %s\n", array_access->name);
    }
    const char *name = symbol_unique_name(symbol);
    outputf("  %%ptr_%d = getelemptr %s, %s\n", temp_sign_index, name,
            index_sign);
    outputf("  store %s, %%ptr_%d\n", value_sign, temp_sign_index);
    temp_sign_index++;
    free((void *)name);
  } else {
    fatalf("不支持的左值类型 %s\n", ast_type_to_string(stmt->lhs->type));
  }
}

static void codegen_var_decl(AstVarDecl *decl) {
  AstVarDef *def = decl->def;
  while (def) {
    Symbol *symbol = new_symbol(def->name, SymbolType_int);
    const char *name = symbol_unique_name(symbol);
    if (def->dimensions.count > 0) {
      int total_count = 1;
      int *dimensions = malloc(sizeof(int) * def->dimensions.count);
      for (int i = 0; i < def->dimensions.count; i++) {
        assert(def->dimensions.elements[i]->type == AST_NUMBER);
        int n = ((AstNumber *)def->dimensions.elements[i])->number;
        total_count *= n;
        dimensions[i] = n;
      }
      assert(total_count > 0);
      symbol->type = SymbolType_array;
      symbol->dimensions = dimensions;
      symbol->dimension_count = def->dimensions.count;
      outputf("  %s = alloc [i32, %d]\n", name, total_count);

      int value_count = 0;
      if (def->val) {
        assert(def->val->type == AST_ARRAY_VALUE);
        AstArrayValue *array_value = (AstArrayValue *)def->val;
        value_count = array_value->count;
        for (int i = 0; i < value_count; i++) {
          AstExp *v = array_value->elements[i];
          codegen_exp(v);
          const char *v_sign = exp_sign(v);
          outputf("  %%%d = getelemptr %s, %d\n", temp_sign_index, name, i);
          outputf("  store %s, %%%d\n", v_sign, temp_sign_index);
          temp_sign_index++;
        }
      }
      // 没有初始化值的元素补 0
      for (int i = value_count; i < total_count; i++) {
        outputf("  %%%d = getelemptr %s, %d\n", temp_sign_index, name, i);
        outputf("  store 0, %%%d\n", temp_sign_index);
        temp_sign_index++;
      }
    } else {
      outputf("  %s = alloc i32\n", name);
      if (def->val) {
        codegen_exp(def->val);
        outputf("  store %s, %s\n", exp_sign(def->val), name);
      }
    }
    free((void *)name);
    def = def->next;
  }
}

static void codegen_const_decl(AstConstDecl *decl) {
  AstConstDef *def = decl->def;
  while (def) {
    if (def->dimensions.count > 0) {
      int total_count = 1;
      int *dimensions = malloc(sizeof(int) * def->dimensions.count);
      for (int i = 0; i < def->dimensions.count; i++) {
        assert(def->dimensions.elements[i]->type == AST_NUMBER);
        int n = ((AstNumber *)def->dimensions.elements[i])->number;
        total_count *= n;
        dimensions[i] = n;
      }
      assert(total_count > 0);
      Symbol *symbol = new_symbol(def->name, SymbolType_array);
      symbol->dimensions = dimensions;
      symbol->dimension_count = def->dimensions.count;
      const char *name = symbol_unique_name(symbol);
      outputf("  %s = alloc [i32, %d]\n", name, total_count);

      int value_count = 0;
      if (def->val) {
        assert(def->val->type == AST_ARRAY_VALUE);
        AstArrayValue *array_value = (AstArrayValue *)def->val;
        value_count = array_value->count;
        for (int i = 0; i < value_count; i++) {
          AstExp *v = array_value->elements[i];
          codegen_exp(v);
          const char *v_sign = exp_sign(v);
          outputf("  %%%d = getelemptr %s, %d\n", temp_sign_index, name, i);
          outputf("  store %s, %%%d\n", v_sign, temp_sign_index);
          temp_sign_index++;
        }
      }
      // 没有初始化值的元素补 0
      for (int i = value_count; i < total_count; i++) {
        outputf("  %%%d = getelemptr %s, %d\n", temp_sign_index, name, i);
        outputf("  store 0, %%%d\n", temp_sign_index);
        temp_sign_index++;
      }
      free((void *)name);
    }
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

static void codegen_while_stmt(AstWhileStmt *stmt) {
  while_index++;
  int current_index = while_index;
  while_body_index = 0;
  int_stack_push(&while_stack, current_index);

  outputf("  jump %%while_entry_%d\n", current_index);
  outputf("\n%%while_entry_%d:\n", current_index);
  codegen_exp(stmt->condition);
  outputf("  br %s, %%while_body_%d, %%while_end_%d\n",
          exp_sign(stmt->condition), current_index, current_index);
  outputf("\n%%while_body_%d:\n", current_index);
  codegen_stmt(stmt->body);
  if (!output_ret_inst) {
    outputf("  jump %%while_entry_%d\n", current_index);
  }
  outputf("\n%%while_end_%d:\n", current_index);

  int_stack_pop(&while_stack);
}

static void codegen_break_stmt(AstBreakStmt *stmt) {
  if (int_stack_empty(&while_stack)) {
    fatalf("break 只能出现在循环内\n");
  }
  outputf("  jump %%while_end_%d\n", int_stack_top(&while_stack));
  while_body_index++;
  int current_index = while_body_index;
  // jump 指令结束了前一个 basic block ，需要新起一个 basic block
  outputf("\n%%while_body_%d_%d:\n", int_stack_top(&while_stack),
          current_index);
}

static void codegen_continue_stmt(AstContinueStmt *stmt) {
  if (int_stack_empty(&while_stack)) {
    fatalf("continue 只能出现在循环内\n");
  }
  outputf("  jump %%while_entry_%d\n", int_stack_top(&while_stack));
  while_body_index++;
  int current_index = while_body_index;
  // jump 指令结束了前一个 basic block ，需要新起一个 basic block
  outputf("\n%%while_body_%d_%d:\n", int_stack_top(&while_stack),
          current_index);
}

static void codegen_stmt(AstStmt *stmt) {
  switch (stmt->type) {
  case AST_BREAK_STMT:
    codegen_break_stmt((AstBreakStmt *)stmt);
    break;
  case AST_CONTINUE_STMT:
    codegen_continue_stmt((AstContinueStmt *)stmt);
    break;
  case AST_WHILE_STMT:
    codegen_while_stmt((AstWhileStmt *)stmt);
    break;
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
    codegen_const_decl((AstConstDecl *)stmt);
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
static void codegen_global_var_decl(AstVarDecl *decl) {
  AstVarDef *def = decl->def;
  while (def) {
    Symbol *symbol = new_symbol(def->name, SymbolType_int);
    const char *name = symbol_unique_name(symbol);
    outputf("global %s = alloc ", name);
    if (def->dimensions.count > 0) {
      int total_count = 1;
      int *dimensions = malloc(sizeof(int) * def->dimensions.count);
      for (int i = 0; i < def->dimensions.count; i++) {
        assert(def->dimensions.elements[i]->type == AST_NUMBER);
        int n = ((AstNumber *)def->dimensions.elements[i])->number;
        total_count *= n;
        dimensions[i] = n;
      }
      assert(total_count > 0);
      symbol->type = SymbolType_array;
      symbol->dimensions = dimensions;
      symbol->dimension_count = def->dimensions.count;
      outputf("[i32, %d], ", total_count);
      if (def->val) {
        assert(def->val->type == AST_ARRAY_VALUE);
        outputf("{");
        AstArrayValue *array_value = (AstArrayValue *)def->val;
        for (int i = 0; i < array_value->count; i++) {
          assert(array_value->elements[i]->type == AST_NUMBER);
          outputf("%d%s", ((AstNumber *)array_value->elements[i])->number,
                  (i == total_count - 1) ? "" : ", ");
        }
        for (int i = array_value->count; i < total_count; i++) {
          outputf("0%s", (i == total_count - 1) ? "" : ", ");
        }
        outputf("}\n");
      } else {
        outputf("zeroinit\n");
      }
    } else {
      outputf("i32, ");
      if (def->val) {
        assert(def->val->type == AST_NUMBER);
        outputf("%d\n", ((AstNumber *)def->val)->number);
      } else {
        outputf("zeroinit\n");
      }
    }
    free((void *)name);
    def = def->next;
  }
}

static void codegen_global_const_decl(AstConstDecl *decl) {
  AstConstDef *def = decl->def;
  while (def) {
    if (def->dimensions.count > 0) {
      int total_count = 1;
      int *dimensions = malloc(sizeof(int) * def->dimensions.count);
      for (int i = 0; i < def->dimensions.count; i++) {
        assert(def->dimensions.elements[i]->type == AST_NUMBER);
        int n = ((AstNumber *)def->dimensions.elements[i])->number;
        total_count *= n;
        dimensions[i] = n;
      }
      assert(total_count > 0);
      Symbol *symbol = new_symbol(def->name, SymbolType_array);
      symbol->is_const_value = true;
      symbol->dimensions = dimensions;
      symbol->dimension_count = def->dimensions.count;
      const char *name = symbol_unique_name(symbol);
      outputf("global %s = alloc [i32, %d], {", name, total_count);
      if (def->val->type != AST_ARRAY_VALUE) {
        fatalf("常量数组的值必须是数组\n");
      }
      AstArrayValue *array_value = (AstArrayValue *)def->val;
      for (int i = 0; i < array_value->count; i++) {
        outputf("%d%s", ((AstNumber *)array_value->elements[i])->number,
                (i == total_count - 1) ? "" : ", ");
      }
      for (int i = array_value->count; i < total_count; i++) {
        outputf("0%s", (i == total_count - 1) ? "" : ", ");
      }
      outputf("}\n");
    }
    def = def->next;
  }
}

static void codegen_func_def(AstFuncDef *func_def) {
  temp_sign_index = 0;
  current_func_def = func_def;
  outputf("fun @%s(", func_def->ident->name);
  if (func_def->param_count > 0) {
    FuncParam *param = func_def->params;
    while (param) {
      outputf("@%s: i32", param->ident->name);
      param = param->next;
      if (param) {
        outputf(", ");
      }
    }
  }
  outputf(") ");
  if (func_def->func_type != BType_VOID) {
    outputf(": i32 ");
  }
  outputf("{\n");

  outputf("%%entry:\n");

  enter_scope();
  FuncParam *param = func_def->params;
  while (param) {
    Symbol *symbol = new_symbol(param->ident->name, SymbolType_int);
    const char *name = symbol_unique_name(symbol);
    outputf("  %s = alloc i32\n", name);
    outputf("  store @%s, %s\n", param->ident->name, name);
    free((void *)name);
    param = param->next;
  }

  codegen_block(func_def->block);
  if (!output_ret_inst) {
    outputf("  ret\n");
  }
  outputf("}\n");
  leave_scope();
}

static void codegen_comp_unit(AstCompUnit *comp_unit) {
  bool has_main = false;
  for (int i = 0; i < comp_unit->count; i++) {
    if (comp_unit->defs[i]->type == AST_FUNC_DEF) {
      AstFuncDef *func_def = (AstFuncDef *)comp_unit->defs[i];
      Symbol *symbol = new_symbol(func_def->ident->name, SymbolType_func);
      update_func_type(func_def, &symbol->func_type);
      codegen_func_def(func_def);
      if (strcmp(func_def->ident->name, "main") == 0 &&
          func_def->func_type == BType_INT) {
        has_main = true;
      }
    } else if (comp_unit->defs[i]->type == AST_VAR_DECL) {
      codegen_global_var_decl((AstVarDecl *)comp_unit->defs[i]);
    } else if (comp_unit->defs[i]->type == AST_CONST_DECL) {
      codegen_global_const_decl((AstConstDecl *)comp_unit->defs[i]);
    } else {
      fatalf("未知的定义类型\n");
    }
  }
  // 在该 CompUnit 中, 必须存在且仅存在一个标识为 main, 无参数, 返回类型为 int
  // 的 FuncDef (函数定义). main 函数是程序的入口点.
  if (!has_main) {
    fatalf("入口函数 main 不存在\n");
  }
}

// 生成 SysY 运行时库的声明
static void codegen_lib_decl(void) {
  Symbol *symbol = NULL;
  outputf("decl @getint(): i32\n");
  symbol = new_symbol("getint", SymbolType_func);
  symbol->func_type.return_type = BType_INT;
  symbol->func_type.param_count = 0;
  symbol->func_type.param_types = NULL;

  outputf("decl @getch(): i32\n");
  symbol = new_symbol("getch", SymbolType_func);
  symbol->func_type.return_type = BType_INT;
  symbol->func_type.param_count = 0;
  symbol->func_type.param_types = NULL;

  outputf("decl @getarray(*i32): i32\n");
  symbol = new_symbol("getarray", SymbolType_func);
  symbol->func_type.return_type = BType_INT;
  symbol->func_type.param_count = 1;
  symbol->func_type.param_types = malloc(sizeof(BType));
  symbol->func_type.param_types[0] = BType_POINTER;

  outputf("decl @putint(i32)\n");
  symbol = new_symbol("putint", SymbolType_func);
  symbol->func_type.return_type = BType_VOID;
  symbol->func_type.param_count = 1;
  symbol->func_type.param_types = malloc(sizeof(BType));
  symbol->func_type.param_types[0] = BType_INT;

  outputf("decl @putch(i32)\n");
  symbol = new_symbol("putch", SymbolType_func);
  symbol->func_type.return_type = BType_VOID;
  symbol->func_type.param_count = 1;
  symbol->func_type.param_types = malloc(sizeof(BType));
  symbol->func_type.param_types[0] = BType_INT;

  outputf("decl @putarray(i32, *i32)\n");
  symbol = new_symbol("putarray", SymbolType_func);
  symbol->func_type.return_type = BType_VOID;
  symbol->func_type.param_count = 2;
  symbol->func_type.param_types = malloc(sizeof(BType) * 2);
  symbol->func_type.param_types[0] = BType_INT;
  symbol->func_type.param_types[1] = BType_POINTER;

  outputf("decl @starttime()\n");
  symbol = new_symbol("starttime", SymbolType_func);
  symbol->func_type.return_type = BType_VOID;
  symbol->func_type.param_count = 0;
  symbol->func_type.param_types = NULL;

  outputf("decl @stoptime()\n");
  symbol = new_symbol("stoptime", SymbolType_func);
  symbol->func_type.return_type = BType_VOID;
  symbol->func_type.param_count = 0;
  symbol->func_type.param_types = NULL;
}

// #endregion

static void init(void) {
  temp_sign_index = 0;
  if_index = 0;
  while_index = 0;
  while_body_index = 0;
  int_stack_init(&while_stack);
  logic_index = 0;
  output_ret_inst = false;
}

void koopa_ir_codegen(AstCompUnit *comp_unit, const char *output_file) {
  init();

  fp = fopen(output_file, "w");
  if (fp == NULL) {
    fprintf(stderr, "无法打开文件 %s\n", output_file);
    exit(1);
  }

  // 优化 AST
  optimize_comp_unit(comp_unit);
  // 生成 IR
  codegen_lib_decl();
  codegen_comp_unit(comp_unit);
  fclose(fp);
}