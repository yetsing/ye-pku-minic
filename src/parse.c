#include "parse.h"

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

static void advance(void) {
  parser.current = parser.next;
  parser.next = next_token();
}

static void consume(TokenType type) {
  if (parser.current.type == type) {
    advance();
  } else {
    fprintf(stderr, "Syntax error: expected %d, got %d at line %d\n", type,
            parser.current.type, parser.current.line);
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
  number->number = strtol(number_str, NULL, base);
  free(number_str);
  consume(TOKEN_INTEGER);
  return number;
}

// Stmt      ::= "return" Number ";";
static AstStmt *parse_stmt(void) {
  AstStmt *stmt = new_ast_stmt();
  match("return");
  stmt->number = parse_number();
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