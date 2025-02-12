#ifndef SRC_AST_H_
#define SRC_AST_H_

typedef enum {
  AST_NUMBER,
  AST_UNARY_EXP,
  AST_BINARY_EXP,
  AST_FUNC_TYPE,
  AST_IDENTIFIER,
  AST_STMT,
  AST_BLOCK,
  AST_FUNC_DEF,
  AST_COMP_UNIT,
} AstType;

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
  AstExp *exp;
} AstStmt;
AstStmt *new_ast_stmt();

typedef struct {
  AstBase base;
  const char *name;
} AstFuncType;
AstFuncType *new_ast_func_type();

typedef struct {
  AstBase base;
  const char *name;
} AstIdentifier;
AstIdentifier *new_ast_identifier();

typedef struct {
  AstBase base;
  AstStmt *stmt;
} AstBlock;
AstBlock *new_ast_block();

typedef struct {
  AstBase base;
  AstFuncType *func_type;
  AstIdentifier *ident;
  AstBlock *block;
} AstFuncDef;
AstFuncDef *new_ast_func_def();

typedef struct {
  AstBase base;
  AstFuncDef *func_def;
} AstCompUnit;
AstCompUnit *new_ast_comp_unit();

#endif // SRC_AST_H_