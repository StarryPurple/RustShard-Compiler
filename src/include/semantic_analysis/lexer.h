#ifndef INSOMNIA_LEXER_H
#define INSOMNIA_LEXER_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace insomnia {

// token type mask
enum class TokenTypeCat : uint32_t {
  SPECIAL     = 1 << 7,
  KEYWORD     = 1 << 8,
  IDENTIFIER  = 1 << 9,
  LITERAL     = 1 << 10,
  // LIFETIME    = 1 << 11,
  PUNCTUATION = 1 << 12,
  DELIMITER   = 1 << 13,
};

enum class TokenType : uint32_t {
  INVALID,

  // special
  UNKNOWN = static_cast<uint32_t>(TokenTypeCat::SPECIAL) + 1,
  END_OF_FILE,

  // keyword
  AS = static_cast<uint32_t>(TokenTypeCat::KEYWORD) + 1,
  BREAK, CONST, CONTINUE, CRATE, ELSE, ENUM, EXTERN, FALSE, FN, FOR, IF, IMPL, IN, LET, LOOP, MATCH, MOD,
  MOVE, MUT, PUB, REF, RETURN, SELF_OBJECT, SELF_TYPE, STATIC, STRUCT, SUPER, TRAIT, TRUE, TYPE, UNSAFE,
  USE, WHERE, WHILE, ASYNC, AWAIT, DYN,

  // identifier
  IDENTIFIER = static_cast<uint32_t>(TokenTypeCat::IDENTIFIER) + 1,

  // literal
  LITERAL = static_cast<uint32_t>(TokenTypeCat::LITERAL) + 1,
  INTEGER_LITERAL, FLOAT_LITERAL, STRING_LITERAL,

  // lifetime

  // LIFETIME = static_cast<uint32_t>(TokenTypeCat::LIFETIME) + 1,

  // punctuation
  PLUS = static_cast<uint32_t>(TokenTypeCat::PUNCTUATION) + 1,
  MINUS, STAR, SLASH, PERCENT, CARET, NOT, AND, OR, AND_AND, OR_OR, SHL, SHR, PLUS_EQ, MINUS_EQ, STAR_EQ,
  SLASH_EQ, PERCENT_EQ, CARET_EQ, AND_EQ, OR_EQ, SHL_EQ, SHR_EQ,EQ, EQ_EQ, NE, GT, LT, GE, LE, AT, UNDERSCORE,
  DOT, DOT_DOT, DOT_DOT_DOT, DOT_DOT_EQ, COMMA, SEMI, COLON, PATH_SEP, R_ARROW, FAT_ARROW, L_ARROW, POUND, DOLLAR,
  QUESTION, TILDE,

  // delimiter
  L_CURLY_BRACE = static_cast<uint32_t>(TokenTypeCat::DELIMITER) + 1,
  R_CURLY_BRACE, L_SQUARE_BRACKET, R_SQUARE_BRACKET, L_PARENTHESIS, R_PARENTHESIS,
};

std::string_view token_type_to_string(TokenType type);

struct Token {
  TokenType token_type;
  std::string_view lexeme;
  std::size_t row, col;
  Token() = default;
  Token(TokenType _token_type, std::string_view _lexeme, std::size_t _row, std::size_t _col)
    : token_type(_token_type), lexeme(_lexeme), row(_row), col(_col) {}

  friend std::ostream& operator<<(std::ostream &os, const Token &token) {
    os << token.row << ' ' << token.col << ' '
       << token_type_to_string(token.token_type) << ' '
       << token.lexeme;
    return os;
  }
};

class Lexer {
  std::string_view _src_code;
  std::vector<Token> _tokens;
  std::size_t _pos = 0; // points to first unhandled character
  std::size_t _row = 1, _col = 1; // exact position of _pos
  bool _is_valid = false;

  // forcefully advance one character
  void advance_one() {
    if(_pos >= _src_code.length()) {
      throw std::runtime_error("Advance out of bound");
    }
    if(_src_code[_pos] == '\n') {
      _pos++; _row++; _col = 1;
    } else {
      _pos++; _col++;
    }
  }

  // filters useless code (white spaces, comments, ...)
  // forward _pos to next meaningful character in _source_code (no matter whether current character is meaningful)
  // if no such character left, set _pos = _code.len()
  // set _row, _col relatively and respectively
  // if detected syntax error (unclosed multi-line comment), return false.
  bool advance();

  // summon token(s), push them into _tokens and advance _pos.
  // should let _pos stop at a handled character.

  bool tokenize_keyword_identifier();
  bool tokenize_string_literal();
  bool tokenize_number_literal();
  bool tokenize_punctuation_delimiter();

public:
  Lexer() = default;
  explicit Lexer(std::string_view code) { tokenize(code); }
  // return false if tokenization fails (syntax error)
  void tokenize(std::string_view code);

  explicit operator bool() const { return _is_valid; }
  [[nodiscard]] bool is_good() const { return _is_valid; }
  [[nodiscard]] std::vector<Token> tokens() const { return _tokens; }
};

}

#endif // INSOMNIA_LEXER_H