#include "tokenize.h"

#include <stdbool.h>

#include "utils.h"

const char *token_type_to_string(TokenType type) {
  switch (type) {
  case TOKEN_IDENTIFIER:
    return "IDENTIFIER";
  case TOKEN_INTEGER:
    return "INTEGER";
  case TOKEN_PLUS:
    return "PLUS";
  case TOKEN_MINUS:
    return "MINUS";
  case TOKEN_ASTERISK:
    return "STAR";
  case TOKEN_SLASH:
    return "SLASH";
  case TOKEN_BANG:
    return "BANG";
  case TOKEN_LPAREN:
    return "LPAREN";
  case TOKEN_RPAREN:
    return "RPAREN";
  case TOKEN_LBRACE:
    return "LBRACE";
  case TOKEN_RBRACE:
    return "RBRACE";
  case TOKEN_SEMICOLON:
    return "SEMICOLON";
  case TOKEN_COMMENT:
    return "COMMENT";
  case TOKEN_EOF:
    return "EOF";
  case TOKEN_PERCENT:
    return "PERCENT";
  case TOKEN_LESS:
    return "LESS";
  case TOKEN_LESS_EQUAL:
    return "LESS_EQUAL";
  case TOKEN_GREATER:
    return "GREATER";
  case TOKEN_GREATER_EQUAL:
    return "GREATER_EQUAL";
  case TOKEN_EQUAL:
    return "EQUAL";
  case TOKEN_NOT_EQUAL:
    return "NOT_EQUAL";
  case TOKEN_AND:
    return "AND";
  case TOKEN_OR:
    return "OR";
  }
  return "UNKNOWN";
}

typedef struct Tokenizer {
  const char *start;
  const char *current;
  int line;
} Tokenizer;

static Tokenizer tokenizer;

void init_tokenizer(const char *input) {
  tokenizer.start = input;
  tokenizer.current = input;
  tokenizer.line = 1;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_octal_digit(char c) { return c >= '0' && c <= '7'; }

static bool is_hex_digit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static bool is_at_end(void) { return *tokenizer.current == '\0'; }

static bool is_identifier_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_identifier(char c) {
  return is_identifier_start(c) || is_digit(c);
}

// Return current character and advance to the next one
static void advance(void) {
  if (!is_at_end()) {
    tokenizer.current++;
  }
}

static char peek(void) { return tokenizer.current[1]; }

static void skip_whitespace(void) {
  while (true) {
    char c = *tokenizer.current;
    if (c == ' ' || c == '\t' || c == '\n') {
      if (c == '\n') {
        tokenizer.line++;
      }
      advance();
    } else {
      break;
    }
  }
}

/*
 * identifier ::= identifier-start (identifier-char)*
 * identifier-start ::= [a-zA-Z_]
 * identifier-char ::= identifier-start | [0-9]
 */
static Token identifier(void) {
  while (is_identifier(*tokenizer.current))
    advance();
  return (Token){TOKEN_IDENTIFIER, tokenizer.start,
                 tokenizer.current - tokenizer.start, tokenizer.line};
}

/*
 * integer-const       ::= decimal-const
 *                       | octal-const
 *                       | hexadecimal-const;
 * decimal-const       ::= nonzero-digit digit*;
 * octal-const         ::= "0" octal-digit*;
 * hexadecimal-const   ::= hexadecimal-prefix hexadecimal-digit+;
 * hexadecimal-prefix  ::= "0x" | "0X";
 */
static Token integer(void) {
  if (*tokenizer.current == '0') {
    advance();
    if (*tokenizer.current == 'x' || *tokenizer.current == 'X') {
      advance();
      while (is_hex_digit(*tokenizer.current))
        advance();
      return (Token){TOKEN_INTEGER, tokenizer.start,
                     tokenizer.current - tokenizer.start, tokenizer.line};
    } else {
      while (is_octal_digit(*tokenizer.current))
        advance();
      return (Token){TOKEN_INTEGER, tokenizer.start,
                     tokenizer.current - tokenizer.start, tokenizer.line};
    }
  } else {
    while (is_digit(*tokenizer.current))
      advance();
    return (Token){TOKEN_INTEGER, tokenizer.start,
                   tokenizer.current - tokenizer.start, tokenizer.line};
  }
}

/*
 * single-line-comment  ::= "//" input-char* NEWLINE;
 */
static Token single_line_comment(void) {
  while (!is_at_end() && *tokenizer.current != '\n') {
    advance();
  }
  return (Token){TOKEN_COMMENT, tokenizer.start,
                 tokenizer.current - tokenizer.start, tokenizer.line};
}

/*
 * multi-line-comment   ::= SLASH STAR input-char* STAR SLASH;
 */
static Token multi_line_comment(void) {
  while (true) {
    if (is_at_end()) {
      fatalf("多行注释没有以 */ 结尾 at line %d\n", tokenizer.line);
    }
    if (*tokenizer.current == '*') {
      advance();
      if (*tokenizer.current == '/') {
        advance();
        break;
      }
    } else {
      advance();
    }
  }
  return (Token){TOKEN_COMMENT, tokenizer.start,
                 tokenizer.current - tokenizer.current, tokenizer.line};
}

Token next_token(void) {
  while (true) {
    skip_whitespace();
    tokenizer.start = tokenizer.current;

    if (is_at_end()) {
      return (Token){TOKEN_EOF, tokenizer.start, 0, tokenizer.line};
    }

    char c = *tokenizer.current;
    switch (c) {
    case '<': {
      if (peek() == '=') {
        advance();
        advance();
        return (Token){TOKEN_LESS_EQUAL, tokenizer.start, 2, tokenizer.line};
      } else {
        advance();
        return (Token){TOKEN_LESS, tokenizer.start, 1, tokenizer.line};
      }
    }
    case '>': {
      if (peek() == '=') {
        advance();
        advance();
        return (Token){TOKEN_GREATER_EQUAL, tokenizer.start, 2, tokenizer.line};
      } else {
        advance();
        return (Token){TOKEN_GREATER, tokenizer.start, 1, tokenizer.line};
      }
    }
    case '=': {
      if (peek() == '=') {
        advance();
        advance();
        return (Token){TOKEN_EQUAL, tokenizer.start, 2, tokenizer.line};
      } else {
        advance();
        return (Token){TOKEN_EQUAL, tokenizer.start, 1, tokenizer.line};
      }
    }
    case '&': {
      if (peek() == '&') {
        advance();
        advance();
        return (Token){TOKEN_AND, tokenizer.start, 2, tokenizer.line};
      } else {
        fatalf("无法识别的字符 %c at line %d\n", c, tokenizer.line);
      }
    }
    case '|': {
      if (peek() == '|') {
        advance();
        advance();
        return (Token){TOKEN_OR, tokenizer.start, 2, tokenizer.line};
      } else {
        fatalf("无法识别的字符 %c at line %d\n", c, tokenizer.line);
      }
    }
    case '%':
      advance();
      return (Token){TOKEN_PERCENT, tokenizer.start, 1, tokenizer.line};
    case '+':
      advance();
      return (Token){TOKEN_PLUS, tokenizer.start, 1, tokenizer.line};
    case '-':
      advance();
      return (Token){TOKEN_MINUS, tokenizer.start, 1, tokenizer.line};
    case '*':
      advance();
      return (Token){TOKEN_ASTERISK, tokenizer.start, 1, tokenizer.line};
    case '!': {
      if (peek() == '=') {
        advance();
        advance();
        return (Token){TOKEN_NOT_EQUAL, tokenizer.start, 2, tokenizer.line};
      } else {
        advance();
        return (Token){TOKEN_BANG, tokenizer.start, 1, tokenizer.line};
      }
    }
    case '/': {
      if (peek() == '/') {
        advance();
        advance();
        single_line_comment();
      } else if (peek() == '*') {
        advance();
        multi_line_comment();
      } else {
        advance();
        return (Token){TOKEN_SLASH, tokenizer.start, 1, tokenizer.line};
      }
      break;
    }
    case '(':
      advance();
      return (Token){TOKEN_LPAREN, tokenizer.start, 1, tokenizer.line};
    case ')':
      advance();
      return (Token){TOKEN_RPAREN, tokenizer.start, 1, tokenizer.line};
    case '{':
      advance();
      return (Token){TOKEN_LBRACE, tokenizer.start, 1, tokenizer.line};
    case '}':
      advance();
      return (Token){TOKEN_RBRACE, tokenizer.start, 1, tokenizer.line};
    case ';':
      advance();
      return (Token){TOKEN_SEMICOLON, tokenizer.start, 1, tokenizer.line};
    default:
      if (is_identifier_start(c)) {
        return identifier();
      }
      if (is_digit(c)) {
        return integer();
      }
      fatalf("无法识别的字符 %c(%d) at line %d\n", c, c, tokenizer.line);
    }
  }
}
