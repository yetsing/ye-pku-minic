#ifndef SRC_AST_H_
#define SRC_AST_H_

#include <stdbool.h>

typedef enum {
  AST_NUMBER,
  AST_ARRAY_VALUE,
  AST_UNARY_EXP,
  AST_BINARY_EXP,
  AST_FUNC_CALL,
  AST_IDENTIFIER,
  AST_ARRAY_ACCESS,
  AST_STMT,
  AST_EXP_STMT,
  AST_CONST_DECL,
  AST_CONST_DEF,
  AST_VAR_DECL,
  AST_VAR_DEF,
  AST_RETURN_STMT,
  AST_ASSIGN_STMT,
  AST_EMPTY_STMT,
  AST_IF_STMT,
  AST_WHILE_STMT,
  AST_BREAK_STMT,
  AST_CONTINUE_STMT,
  AST_BLOCK,
  AST_FUNC_DEF,
  AST_COMP_UNIT,
} AstType;
const char *ast_type_to_string(AstType type);

typedef enum {
  BType_UNKNOWN,
  BType_INT,
  BType_VOID,
  BType_POINTER,
} BType;
const char *btype_to_string(BType type);

typedef enum {
  BinaryOpType_ADD,
  BinaryOpType_SUB,
  BinaryOpType_MUL,
  BinaryOpType_DIV,
  BinaryOpType_MOD,
  BinaryOpType_EQ,
  BinaryOpType_NE, // Not Equal
  BinaryOpType_LT, // Less Than
  BinaryOpType_LE, // Less Than or Equal
  BinaryOpType_GT, // Greater Than
  BinaryOpType_GE, // Greater Than or Equal
  BinaryOpType_AND,
  BinaryOpType_OR,
} BinaryOpType;
const char *binary_op_type_to_string(BinaryOpType type);

typedef struct AstBase AstBase;

typedef void (*DumpFunc)(AstBase *node, int indent);

typedef struct AstBase {
  AstType type;
  DumpFunc dump;
} AstBase;

typedef struct {
  AstType type;
  DumpFunc dump;
} AstExp;

typedef struct {
  AstExp base;
  int number;
} AstNumber;
AstNumber *new_ast_number();

typedef struct {
  AstExp base;
  AstExp **elements;
  int count;
  int capacity;
} AstArrayValue;
AstArrayValue *new_ast_array_value();
void ast_array_value_add(AstArrayValue *array_value, AstExp *element);

typedef struct {
  AstExp base;
  const char *name;
} AstIdentifier;
AstIdentifier *new_ast_identifier();

typedef struct {
  AstExp base;
  const char *name;
  AstExp *index;
} AstArrayAccess;
AstArrayAccess *new_ast_array_access();

typedef struct AstUnaryExp {
  AstExp base;
  char op;
  AstExp *operand;
} AstUnaryExp;
AstUnaryExp *new_ast_unary_exp();

typedef struct AstBinaryExp {
  AstExp base;
  BinaryOpType op;
  AstExp *lhs;
  AstExp *rhs;
} AstBinaryExp;
AstBinaryExp *new_ast_binary_exp();

typedef struct {
  AstBase base;
  AstIdentifier *ident;
  AstExp **args;
  int count;
  int capacity;
} AstFuncCall;
AstFuncCall *new_ast_func_call();
void ast_func_call_add(AstFuncCall *func_call, AstExp *arg);

typedef struct AstStmt AstStmt;
typedef struct AstStmt {
  AstType type;
  DumpFunc dump;
  AstStmt *next; // 使用链表结构存储多个语句
} AstStmt;

typedef struct {
  AstStmt base;
} AstBreakStmt;
AstBreakStmt *new_ast_break_stmt();

typedef struct {
  AstStmt base;
} AstContinueStmt;
AstContinueStmt *new_ast_continue_stmt();

typedef struct {
  AstStmt base;
  AstExp *condition;
  AstStmt *body;
} AstWhileStmt;
AstWhileStmt *new_ast_while_stmt();

typedef struct {
  AstStmt base;
  AstExp *condition;
  AstStmt *then;
  AstStmt *else_;
} AstIfStmt;
AstIfStmt *new_ast_if_stmt();

typedef struct {
  AstStmt base;
} AstEmptyStmt;
AstEmptyStmt *new_ast_empty_stmt();

typedef struct {
  AstStmt base;
  AstExp *exp;
} AstExpStmt;
AstExpStmt *new_ast_exp_stmt();

typedef struct {
  AstStmt base;
  AstExp *exp;
} AstReturnStmt;
AstReturnStmt *new_ast_return_stmt();

typedef struct {
  AstStmt base;
  AstExp *lhs;
  AstExp *exp;
} AstAssignStmt;
AstAssignStmt *new_ast_assign_stmt();

typedef struct AstConstDef AstConstDef;
typedef struct AstConstDef {
  AstBase base;
  const char *name;
  AstExp *array_size;
  AstExp *val;
  AstConstDef *next; // 使用链表结构存储多个常量定义
} AstConstDef;
AstConstDef *new_ast_const_def();

typedef struct {
  AstStmt base;
  BType type;
  AstConstDef *def;
} AstConstDecl;
AstConstDecl *new_ast_const_decl();

typedef struct AstVarDef AstVarDef;
typedef struct AstVarDef {
  AstBase base;
  const char *name;
  AstExp *array_size;
  AstExp *val;
  AstVarDef *next;
} AstVarDef;
AstVarDef *new_ast_var_def();

typedef struct {
  AstStmt base;
  BType type;
  AstVarDef *def;
} AstVarDecl;
AstVarDecl *new_ast_var_decl();

typedef struct {
  AstStmt base;
  AstStmt *stmt;
} AstBlock;
AstBlock *new_ast_block();

typedef struct FuncParam FuncParam;
typedef struct FuncParam {
  BType type;
  AstIdentifier *ident;
  FuncParam *next;
} FuncParam;

typedef struct {
  AstBase base;
  BType func_type;
  AstIdentifier *ident;
  AstBlock *block;
  FuncParam *params;
  int param_count;
} AstFuncDef;
AstFuncDef *new_ast_func_def();

typedef struct {
  AstBase base;
  AstBase **defs;
  int count;
  int capacity;
} AstCompUnit;
AstCompUnit *new_ast_comp_unit();
void ast_comp_unit_add(AstCompUnit *comp_unit, AstBase *node);

#endif // SRC_AST_H_