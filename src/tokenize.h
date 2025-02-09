#ifndef SRC_TOKENIZE_H_
#define SRC_TOKENIZE_H_

typedef enum {
  TOKEN_EOF,
  TOKEN_COMMENT,
  TOKEN_INTEGER,
  TOKEN_IDENTIFIER,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  TOKEN_SEMICOLON,
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

void init_tokenizer(const char *input);
Token next_token(void);

#endif // SRC_TOKENIZE_H_