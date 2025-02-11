#include "parse.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "tokenize.h"

typedef struct {
  Token current;
  Token next;
} Parser;

static Parser parser;

static AstExp *parse_exp(void);
static AstExp *parse_primary_exp(void);
static AstNumber *parse_number(void);
static AstExp *parse_unary_exp(void);
static AstExp *parse_add_exp(void);
static AstExp *parse_mul_exp(void);

static void advance(void) {
  parser.current = parser.next;
  parser.next = next_token();
}

static void consume(TokenType type) {
  if (parser.current.type == type) {
    advance();
  } else {
    fprintf(stderr, "Syntax error: expected %s, got %s at line %d\n",
            token_type_to_string(type),
            token_type_to_string(parser.current.type), parser.current.line);
    exit(1);
  }
}

static void match(const char *expected) {
  if (strncmp(parser.current.start, expected, parser.current.length) == 0) {
    advance();
  } else {
    fprintf(stderr, "Syntax error: expected %s, got %.*s at line %d\n",
            expected, parser.current.length, parser.current.start,
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

// PrimaryExp  ::= "(" Exp ")" | Number;
static AstExp *parse_primary_exp(void) {
  switch (parser.current.type) {
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

// UnaryExp   ::= PrimaryExp | ("+" | "-" | "!") UnaryExp;
static AstExp *parse_unary_exp(void) {
  if (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS ||
      parser.current.type == TOKEN_BANG) {
    AstUnaryExp *unary_exp = new_ast_unary_exp();
    unary_exp->op = *parser.current.start;
    advance();
    unary_exp->operand = parse_unary_exp();
    return (AstExp *)unary_exp;
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
    binary_exp->op = *parser.current.start;
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
    binary_exp->op = *parser.current.start;
    advance();
    binary_exp->rhs = parse_mul_exp();
    exp = (AstExp *)binary_exp;
  }
  return exp;
}

// Exp         ::= AddExp;
static AstExp *parse_exp(void) { return (AstExp *)parse_add_exp(); }

// Stmt        ::= "return" Exp ";";
static AstStmt *parse_stmt(void) {
  AstStmt *stmt = new_ast_stmt();
  match("return");
  stmt->exp = parse_exp();
  consume(TOKEN_SEMICOLON);
  return stmt;
}

// FuncType  ::= "int";
static AstFuncType *parse_func_type(void) {
  AstFuncType *func_type = new_ast_func_type();
  func_type->name = strndup(parser.current.start, parser.current.length);
  match("int");
  return func_type;
}

// IDENT;
static AstIdentifier *parse_identifier(void) {
  AstIdentifier *ident = new_ast_identifier();
  ident->name = strndup(parser.current.start, parser.current.length);
  consume(TOKEN_IDENTIFIER);
  return ident;
}

// Block     ::= "{" Stmt "}";
static AstBlock *parse_block(void) {
  consume(TOKEN_LBRACE);
  AstBlock *block = new_ast_block();
  block->stmt = parse_stmt();
  consume(TOKEN_RBRACE);
  return block;
}

// FuncDef   ::= FuncType IDENT "(" ")" Block;
static AstFuncDef *parse_func_def(void) {
  AstFuncDef *func_def = new_ast_func_def();
  func_def->func_type = parse_func_type();
  func_def->ident = parse_identifier();
  consume(TOKEN_LPAREN);
  consume(TOKEN_RPAREN);
  func_def->block = parse_block();
  return func_def;
}

// CompUnit  ::= FuncDef;
static AstCompUnit *parse_comp_unit(void) {
  AstCompUnit *comp_unit = new_ast_comp_unit();
  comp_unit->func_def = parse_func_def();
  return comp_unit;
}

static void init_parser(void) {
  advance();
  advance();
}

AstCompUnit *parse(const char *input) {
  init_tokenizer(input);
  init_parser();
  return parse_comp_unit();
}