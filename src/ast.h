#ifndef SRC_AST_H_
#define SRC_AST_H_

typedef enum {
  AST_NUMBER,
  AST_UNARY_EXP,
  AST_FUNC_TYPE,
  AST_IDENTIFIER,
  AST_STMT,
  AST_BLOCK,
  AST_FUNC_DEF,
  AST_COMP_UNIT,
} AstType;

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