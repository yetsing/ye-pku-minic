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
      Symbol *symbol = new_symbol(def->name, SymbolType_int);
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
    break;
  }
  case AST_WHILE_STMT: {
    AstWhileStmt *while_stmt = (AstWhileStmt *)stmt;
    while_stmt->condition = optimize_exp(while_stmt->condition);
    optimize_stmt(while_stmt->body);
    break;
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
  for (int i = 0; i < comp_unit->count; i++) {
    AstFuncDef *func_def = (AstFuncDef *)comp_unit->func_defs[i];
    optimize_block(func_def->block);
  }

  // 重置符号表
  reset_symbol_table();

  printf("    === 优化后的 AST ===\n");
  comp_unit->base.dump((AstBase *)comp_unit, 4);
  printf("\n");
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
    Symbol *symbol = new_symbol(def->name, SymbolType_int);
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

  // if (func_def->func_type == BType_VOID) {
  //   if (func_def->param_count == 0) {
  //     outputf("fun @%s() {\n", func_def->ident->name);
  //   } else {
  //     outputf("fun @%s(", func_def->ident->name);
  //     FuncParam *param = func_def->params;
  //     while (param) {
  //       outputf("@%s: i32", param->ident->name);
  //       param = param->next;
  //       if (param) {
  //         outputf(", ");
  //       }
  //     }
  //     outputf(") {\n");
  //   }
  // } else {
  //   if (func_def->param_count == 0) {
  //     outputf("fun @%s(): i32 {\n", func_def->ident->name);
  //   } else {
  //     outputf("fun @%s(", func_def->ident->name);
  //     FuncParam *param = func_def->params;
  //     while (param) {
  //       outputf("@%s: i32", param->ident->name);
  //       param = param->next;
  //       if (param) {
  //         outputf(", ");
  //       }
  //     }
  //     outputf("): i32 {\n");
  //   }
  // }
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
    AstFuncDef *func_def = (AstFuncDef *)comp_unit->func_defs[i];
    Symbol *symbol = new_symbol(func_def->ident->name, SymbolType_func);
    update_func_type(func_def, &symbol->func_type);
    codegen_func_def(func_def);
    if (strcmp(func_def->ident->name, "main") == 0 &&
        func_def->func_type == BType_INT) {
      has_main = true;
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