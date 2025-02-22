#include "parse.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "tokenize.h"
#include "utils.h"

typedef struct {
  Token current;
  Token next;
  Token next2;
} Parser;

static Parser parser;

static AstExp *parse_exp(void);
static AstExp *parse_primary_exp(void);
static AstNumber *parse_number(void);
static AstIdentifier *parse_identifier(void);
static AstExp *parse_unary_exp(void);
static AstExp *parse_call_exp(void);
static AstExp *parse_add_exp(void);
static AstExp *parse_mul_exp(void);
static AstExp *parse_lor_exp(void);
static AstExp *parse_land_exp(void);
static AstExp *parse_eq_exp(void);
static AstExp *parse_rel_exp(void);
static AstStmt *parse_stmt(void);
static AstBlock *parse_block(void);

static void advance(void) {
  parser.current = parser.next;
  parser.next = parser.next2;
  parser.next2 = next_token();
}

static void consume(TokenType type) {
  if (parser.current.type == type) {
    advance();
  } else {
    fatalf("Syntax error: expected %s, got %s(%.*s) at line %d\n",
           token_type_to_string(type),
           token_type_to_string(parser.current.type), parser.current.length,
           parser.current.start, parser.current.line);
  }
}

static bool try_consume(TokenType type) {
  if (parser.current.type == type) {
    advance();
    return true;
  }
  return false;
}

static void match(const char *expected) {
  if (strlen(expected) == parser.current.length &&
      strncmp(parser.current.start, expected, parser.current.length) == 0) {
    advance();
  } else {
    fatalf("Syntax error: expected %s, got %.*s at line %d\n", expected,
           parser.current.length, parser.current.start, parser.current.line);
  }
}

__attribute__((unused)) static bool try_match(const char *expected) {
  if (strncmp(parser.current.start, expected, parser.current.length) == 0) {
    advance();
    return true;
  }
  return false;
}

static bool current_is(TokenType type) { return parser.current.type == type; }
static bool current_eq(const char *s) {
  return strlen(s) == parser.current.length &&
         strncmp(parser.current.start, s, parser.current.length) == 0;
}

static bool peek_is(TokenType type) { return parser.next.type == type; }
__attribute__((unused)) static bool peek_eq(const char *s) {
  return strlen(s) == parser.next.length &&
         strncmp(parser.next.start, s, parser.next.length) == 0;
}

static bool peek2_is(TokenType type) { return parser.next2.type == type; }

BinaryOpType token_type_to_binary_op_type(TokenType type) {
  switch (type) {
  case TOKEN_PLUS:
    return BinaryOpType_ADD;
  case TOKEN_MINUS:
    return BinaryOpType_SUB;
  case TOKEN_ASTERISK:
    return BinaryOpType_MUL;
  case TOKEN_SLASH:
    return BinaryOpType_DIV;
  case TOKEN_PERCENT:
    return BinaryOpType_MOD;
  case TOKEN_EQUAL:
    return BinaryOpType_EQ;
  case TOKEN_NOT_EQUAL:
    return BinaryOpType_NE;
  case TOKEN_LESS:
    return BinaryOpType_LT;
  case TOKEN_LESS_EQUAL:
    return BinaryOpType_LE;
  case TOKEN_GREATER:
    return BinaryOpType_GT;
  case TOKEN_GREATER_EQUAL:
    return BinaryOpType_GE;
  case TOKEN_AND:
    return BinaryOpType_AND;
  case TOKEN_OR:
    return BinaryOpType_OR;
  default:
    fprintf(stderr, "Invalid binary operator: %d at line %d\n", type,
            parser.current.line);
    exit(1);
  }
}

// Number    ::= INT_CONST;
static AstNumber *parse_number(void) {
  AstNumber *number = new_ast_number();
  int base = 10;
  const char *start = parser.current.start;
  if (parser.current.start[0] == '0' && parser.current.length > 1) {
    base = 8;
    if (parser.current.length > 2 &&
        (parser.current.start[1] == 'x' || parser.current.start[1] == 'X')) {
      base = 16;
    }
  }
  char *number_str = strndup(start, parser.current.length);
  char *endptr;
  long long n = strtoll(number_str, &endptr, base);
  if (*endptr != '\0') {
    fprintf(stderr, "无效数字: %.*s at line %d\n", parser.current.length,
            parser.current.start, parser.current.line);
    exit(1);
  }
  // i32 number range check
  if (n > INT32_MAX || n < INT32_MIN) {
    fprintf(stderr, "数字超出 i32 范围: %lld (base %d) at line %d\n", n, base,
            parser.current.line);
    exit(1);
  }
  free(number_str);
  number->number = n;
  consume(TOKEN_INTEGER);
  return number;
}

// PrimaryExp  ::= "(" Exp ")" | IDENT | Number;
static AstExp *parse_primary_exp(void) {
  switch (parser.current.type) {
  case TOKEN_IDENTIFIER:
    return (AstExp *)parse_identifier();
  case TOKEN_INTEGER:
    return (AstExp *)parse_number();
  case TOKEN_LPAREN:
    advance();
    AstExp *exp = parse_exp();
    consume(TOKEN_RPAREN);
    return exp;
  default:
    fprintf(stderr, "Syntax error: unexpected token %d at line %d\n",
            parser.current.type, parser.current.line);
    exit(1);
  }
}

// CallExp     ::= IDENT "(" [Exp {"," Exp}] ")";
static AstExp *parse_call_exp(void) {
  AstFuncCall *func_call = new_ast_func_call();
  func_call->ident = parse_identifier();
  consume(TOKEN_LPAREN);
  if (!try_consume(TOKEN_RPAREN)) {
    do {
      AstExp *exp = parse_exp();
      ast_func_call_add(func_call, exp);
    } while (try_consume(TOKEN_COMMA));
    consume(TOKEN_RPAREN);
  }
  return (AstExp *)func_call;
}

// UnaryExp   ::= PrimaryExp | CallExp | ("+" | "-" | "!") UnaryExp;
static AstExp *parse_unary_exp(void) {
  if (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS ||
      parser.current.type == TOKEN_BANG) {
    AstUnaryExp *unary_exp = new_ast_unary_exp();
    unary_exp->op = *parser.current.start;
    advance();
    unary_exp->operand = parse_unary_exp();
    return (AstExp *)unary_exp;
  } else if (parser.current.type == TOKEN_IDENTIFIER && peek_is(TOKEN_LPAREN)) {
    return parse_call_exp();
  }

  return parse_primary_exp();
}

// MulExp      ::= UnaryExp (("*" | "/" | "%") UnaryExp)*;
static AstExp *parse_mul_exp(void) {
  AstExp *exp = parse_unary_exp();
  while (parser.current.type == TOKEN_ASTERISK ||
         parser.current.type == TOKEN_SLASH ||
         parser.current.type == TOKEN_PERCENT) {
    AstBinaryExp *binary_exp = new_ast_binary_exp();
    binary_exp->lhs = exp;
    binary_exp->op = token_type_to_binary_op_type(parser.current.type);
    advance();
    binary_exp->rhs = parse_unary_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// AddExp      ::= MulExp (("+" | "-") MulExp)*;
static AstExp *parse_add_exp(void) {
  AstExp *exp = parse_mul_exp();
  while (parser.current.type == TOKEN_PLUS ||
         parser.current.type == TOKEN_MINUS) {
    AstBinaryExp *binary_exp = new_ast_binary_exp();
    binary_exp->lhs = exp;
    binary_exp->op = token_type_to_binary_op_type(parser.current.type);
    advance();
    binary_exp->rhs = parse_mul_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// RelExp      ::= AddExp (("<" | "<=" | ">" | ">=") AddExp)*;
static AstExp *parse_rel_exp(void) {
  AstExp *exp = parse_add_exp();
  while (parser.current.type == TOKEN_LESS ||
         parser.current.type == TOKEN_LESS_EQUAL ||
         parser.current.type == TOKEN_GREATER ||
         parser.current.type == TOKEN_GREATER_EQUAL) {
    AstBinaryExp *binary_exp = new_ast_binary_exp();
    binary_exp->lhs = exp;
    binary_exp->op = token_type_to_binary_op_type(parser.current.type);
    advance();
    binary_exp->rhs = parse_add_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// EqExp       ::= RelExp (("==" | "!=") RelExp)*;
static AstExp *parse_eq_exp(void) {
  AstExp *exp = parse_rel_exp();
  while (parser.current.type == TOKEN_EQUAL ||
         parser.current.type == TOKEN_NOT_EQUAL) {
    AstBinaryExp *binary_exp = new_ast_binary_exp();
    binary_exp->lhs = exp;
    binary_exp->op = token_type_to_binary_op_type(parser.current.type);
    advance();
    binary_exp->rhs = parse_rel_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// LAndExp     ::= EqExp ("&&" EqExp)*;
static AstExp *parse_land_exp(void) {
  AstExp *exp = parse_eq_exp();
  while (parser.current.type == TOKEN_AND) {
    AstBinaryExp *binary_exp = new_ast_binary_exp();
    binary_exp->lhs = exp;
    binary_exp->op = token_type_to_binary_op_type(parser.current.type);
    advance();
    binary_exp->rhs = parse_eq_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// LOrExp      ::= LAndExp ("||" LAndExp)*;
static AstExp *parse_lor_exp(void) {
  AstExp *exp = parse_land_exp();
  while (parser.current.type == TOKEN_OR) {
    AstBinaryExp *binary_exp = new_ast_binary_exp();
    binary_exp->lhs = exp;
    binary_exp->op = token_type_to_binary_op_type(parser.current.type);
    advance();
    binary_exp->rhs = parse_land_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// Exp         ::= LOrExp;
static AstExp *parse_exp(void) { return parse_lor_exp(); }

// ReturnStmt        ::= "return" Exp ";";
static AstStmt *parse_return_stmt(void) {
  AstReturnStmt *stmt = new_ast_return_stmt();
  match("return");
  stmt->exp = parse_exp();
  consume(TOKEN_SEMICOLON);
  return (AstStmt *)stmt;
}

// AssignStmt        ::= IDENT "=" Exp ";";
static AstStmt *parse_assign_stmt(void) {
  AstAssignStmt *stmt = new_ast_assign_stmt();
  stmt->lhs = (AstExp *)parse_identifier();
  consume(TOKEN_ASSIGN);
  stmt->exp = parse_exp();
  consume(TOKEN_SEMICOLON);
  return (AstStmt *)stmt;
}

// FuncType  ::= "int";
static BType parse_func_type(void) {
  if (try_match("void")) {
    return BType_VOID;
  } else {
    match("int");
    return BType_INT;
  }
}

// IDENT;
static AstIdentifier *parse_identifier(void) {
  AstIdentifier *ident = new_ast_identifier();
  ident->name = strndup(parser.current.start, parser.current.length);
  consume(TOKEN_IDENTIFIER);
  return ident;
}

// ConstDecl   ::= "const" "int" IDENT "=" Exp ("," IDENT "=" Exp)* ";";
static AstConstDecl *parse_const_decl(void) {
  AstConstDecl *const_decl = new_ast_const_decl();
  match("const");
  match("int");
  const_decl->type = BType_INT;
  AstConstDef head;
  AstConstDef *tail = &head;
  do {
    AstConstDef *def = new_ast_const_def();
    def->name = strndup(parser.current.start, parser.current.length);
    consume(TOKEN_IDENTIFIER);
    match("=");
    def->exp = parse_exp();
    tail->next = def;
    tail = def;
  } while (try_consume(TOKEN_COMMA));
  consume(TOKEN_SEMICOLON);
  const_decl->def = head.next;
  return const_decl;
}

// VarDecl       ::= "int" VarDef ("," VarDef)* ";";
// VarDef        ::= IDENT | IDENT "=" InitVal;
static AstVarDecl *parse_var_decl(void) {
  AstVarDecl *var_decl = new_ast_var_decl();
  match("int");
  var_decl->type = BType_INT;
  AstVarDef head;
  AstVarDef *tail = &head;
  do {
    AstVarDef *def = new_ast_var_def();
    def->name = strndup(parser.current.start, parser.current.length);
    consume(TOKEN_IDENTIFIER);
    if (try_consume(TOKEN_ASSIGN)) {
      def->exp = parse_exp();
    }
    tail->next = def;
    tail = def;
  } while (try_consume(TOKEN_COMMA));
  consume(TOKEN_SEMICOLON);
  var_decl->def = head.next;
  return var_decl;
}

// ExpStmt       ::= Exp ";";
static AstExpStmt *parse_exp_stmt(void) {
  AstExpStmt *stmt = new_ast_exp_stmt();
  stmt->exp = parse_exp();
  consume(TOKEN_SEMICOLON);
  return stmt;
}

// IfStmt       ::= "if" "(" Exp ")" Stmt ("else" Stmt)?;
static AstIfStmt *parse_if_stmt(void) {
  AstIfStmt *stmt = new_ast_if_stmt();
  match("if");
  consume(TOKEN_LPAREN);
  stmt->condition = parse_exp();
  consume(TOKEN_RPAREN);
  stmt->then = parse_stmt();
  if (try_match("else")) {
    stmt->else_ = parse_stmt();
  }
  return stmt;
}

// WhileStmt     ::= "while" "(" Exp ")" Stmt;
static AstWhileStmt *parse_while_stmt(void) {
  AstWhileStmt *stmt = new_ast_while_stmt();
  match("while");
  consume(TOKEN_LPAREN);
  stmt->condition = parse_exp();
  consume(TOKEN_RPAREN);
  stmt->body = parse_stmt();
  return stmt;
}

// BreakStmt     ::= "break" ";" ;
static AstBreakStmt *parse_break_stmt(void) {
  AstBreakStmt *stmt = new_ast_break_stmt();
  match("break");
  consume(TOKEN_SEMICOLON);
  return stmt;
}

// ContinueStmt  ::= "continue" ";" ;
static AstContinueStmt *parse_continue_stmt(void) {
  AstContinueStmt *stmt = new_ast_continue_stmt();
  match("continue");
  consume(TOKEN_SEMICOLON);
  return stmt;
}

// Stmt          ::= AssignStmt
//                 | Block
//                 | ExpStmt
//                 | ";"
//                 | IfStmt
//                 | BreakStmt
//                 | ContinueStmt
//                 | WhileStmt
//                 | ReturnStmt;
static AstStmt *parse_stmt(void) {
  if (current_eq("return")) {
    return parse_return_stmt();
  } else if (current_is(TOKEN_IDENTIFIER) && peek_is(TOKEN_ASSIGN)) {
    return parse_assign_stmt();
  } else if (current_is(TOKEN_LBRACE)) {
    return (AstStmt *)parse_block();
  } else if (current_is(TOKEN_SEMICOLON)) {
    advance();
    return (AstStmt *)new_ast_empty_stmt();
  } else if (current_eq("if")) {
    return (AstStmt *)parse_if_stmt();
  } else if (current_eq("while")) {
    return (AstStmt *)parse_while_stmt();
  } else if (current_eq("continue")) {
    return (AstStmt *)parse_continue_stmt();
  } else if (current_eq("break")) {
    return (AstStmt *)parse_break_stmt();
  } else {
    return (AstStmt *)parse_exp_stmt();
  }
}

// Decl          ::= ConstDecl | VarDecl;
static AstStmt *parse_decl(void) {
  if (current_eq("const")) {
    return (AstStmt *)parse_const_decl();
  } else if (current_eq("int")) {
    return (AstStmt *)parse_var_decl();
  } else {
    fatalf("Syntax error: expected const or int, got %.*s at line %d\n",
           parser.current.length, parser.current.start, parser.current.line);
    return NULL;
  }
}

// BlockItem     :: = Decl | Stmt;
static AstStmt *parse_block_item(void) {
  if (current_eq("const") || current_eq("int")) {
    return parse_decl();
  } else {
    return parse_stmt();
  }
}

// Block         ::= "{" {BlockItem} "}";
static AstBlock *parse_block(void) {
  consume(TOKEN_LBRACE);
  AstBlock *block = new_ast_block();
  AstStmt head;
  head.next = NULL;
  AstStmt *tail = &head;
  while (!current_is(TOKEN_RBRACE)) {
    AstStmt *stmt = parse_block_item();
    tail->next = stmt;
    tail = stmt;
  }
  block->stmt = head.next;
  consume(TOKEN_RBRACE);
  return block;
}

// FuncDef   ::= FuncType IDENT "(" [FuncFParams] ")" Block;
// FuncFParams ::= FuncFParam ("," FuncFParam)*;
// FuncFParam  ::= "int" IDENT;
static AstFuncDef *parse_func_def(void) {
  AstFuncDef *func_def = new_ast_func_def();
  func_def->func_type = parse_func_type();
  func_def->ident = parse_identifier();
  consume(TOKEN_LPAREN);
  if (!try_consume(TOKEN_RPAREN)) {
    FuncParam head;
    head.next = NULL;
    FuncParam *tail = &head;
    do {
      FuncParam *param = calloc(1, sizeof(FuncParam));
      param->type = parse_func_type();
      param->ident = parse_identifier();
      tail->next = param;
      tail = param;
      func_def->param_count++;
    } while (try_consume(TOKEN_COMMA));
    func_def->params = head.next;
    consume(TOKEN_RPAREN);
  }
  func_def->block = parse_block();
  return func_def;
}

// CompUnit  ::= (Decl | FuncDef)+;
static AstCompUnit *parse_comp_unit(void) {
  AstCompUnit *comp_unit = new_ast_comp_unit();
  while (!current_is(TOKEN_EOF)) {
    if (current_eq("const") ||
        (current_eq("int") && peek_is(TOKEN_IDENTIFIER) &&
         !peek2_is(TOKEN_LPAREN))) {
      AstStmt *decl = parse_decl();
      ast_comp_unit_add(comp_unit, (AstBase *)decl);
    } else {
      AstFuncDef *func_def = parse_func_def();
      ast_comp_unit_add(comp_unit, (AstBase *)func_def);
    }
  }
  consume(TOKEN_EOF);
  return comp_unit;
}

static void init_parser(void) {
  advance();
  advance();
  advance();
}

AstCompUnit *parse(const char *input) {
  init_tokenizer(input);
  init_parser();
  return parse_comp_unit();
}