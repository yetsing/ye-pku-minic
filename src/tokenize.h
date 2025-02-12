#ifndef SRC_TOKENIZE_H_
#define SRC_TOKENIZE_H_

typedef enum {
  TOKEN_EOF,
  TOKEN_COMMENT,
  TOKEN_INTEGER,
  TOKEN_IDENTIFIER,
  TOKEN_PLUS,          // +
  TOKEN_MINUS,         // -
  TOKEN_ASTERISK,      // *
  TOKEN_SLASH,         // /
  TOKEN_PERCENT,       // %
  TOKEN_LPAREN,        // (
  TOKEN_RPAREN,        // )
  TOKEN_LBRACE,        // {
  TOKEN_RBRACE,        // }
  TOKEN_SEMICOLON,     // ;
  TOKEN_BANG,          // !
  TOKEN_LESS,          // <
  TOKEN_LESS_EQUAL,    // <=
  TOKEN_GREATER,       // >
  TOKEN_GREATER_EQUAL, // >=
  TOKEN_EQUAL,         // ==
  TOKEN_NOT_EQUAL,     // !=
  TOKEN_AND,           // &&
  TOKEN_OR,            // ||
} TokenType;

const char *token_type_to_string(TokenType type);

typedef struct {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

void init_tokenizer(const char *input);
Token next_token(void);

#endif // SRC_TOKENIZE_H_