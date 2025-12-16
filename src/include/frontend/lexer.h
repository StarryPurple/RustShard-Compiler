#ifndef INSOMNIA_LEXER_H
#define INSOMNIA_LEXER_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

#include "common.h"

namespace insomnia::rust_shard {
enum class TokenTypeCat : uint32_t {
  kSpecial = 1 << 7,
  kKeyword = 1 << 8,
  kIdentifier = 1 << 9,
  kLiteral = 1 << 10,
  // LIFETIME = 1 << 11,
  kPunctuation = 1 << 12,
  kDelimiter = 1 << 13,
};

enum class TokenType : uint32_t {
  kInvalid,

  // special
  kUnknown = static_cast<uint32_t>(TokenTypeCat::kSpecial) + 1,
  kEndOfFile,

  // keyword
  kAs = static_cast<uint32_t>(TokenTypeCat::kKeyword) + 1,
  kBreak, kConst, kContinue, kCrate, kElse, kEnum, kExtern, kFalse, kFn, kFor, kIf, kImpl, kIn, kLet, kLoop, kMatch, kMod,
  kMove, kMut, kPub, kRef, kReturn, kSelfObject, kSelfType, kStatic, kStruct, kSuper, kTrait, kTrue, kType, kUnsafe,
  kUse, kWhere, kWhile, kAsync, kAwait, kDyn,

  // identifier
  kIdentifier = static_cast<uint32_t>(TokenTypeCat::kIdentifier) + 1,

  // literal
  kLiteral = static_cast<uint32_t>(TokenTypeCat::kLiteral) + 1,
  kIntegerLiteral, kFloatLiteral, kStringLiteral, kCharLiteral,

  // lifetime
  // kLifetime = static_cast<uint32_t>(TokenTypeCat::kLifetime) + 1,

  // punctuation
  kPlus = static_cast<uint32_t>(TokenTypeCat::kPunctuation) + 1,
  kMinus, kStar, kSlash, kPercent, kCaret, kNot, kAnd, kOr, kAndAnd, kOrOr, kShl, kShr, kPlusEq, kMinusEq, kStarEq,
  kSlashEq, kPercentEq, kCaretEq, kAndEq, kOrEq, kShlEq, kShrEq, kEq, kEqEq, kNe, kGt, kLt, kGe, kLe, kAt, kUnderscore,
  kDot, kDotDot, kDotDotDot, kDotDotEq, kComma, kSemi, kColon, kPathSep, kRArrow, kFatArrow, kLArrow, kPound, kDollar,
  kQuestion, kTilde,

  // delimiter
  kLCurlyBrace = static_cast<uint32_t>(TokenTypeCat::kDelimiter) + 1,
  kRCurlyBrace, kLSquareBracket, kRSquareBracket, kLParenthesis, kRParenthesis,
};

inline TokenTypeCat get_token_category(TokenType type) {
  return static_cast<TokenTypeCat>(static_cast<uint32_t>(type) & 0xff00);
}

std::string token_type_to_string(TokenType type);

struct Token {
  TokenType type = TokenType::kInvalid;
  StringRef lexeme;
  std::size_t row = -1, col = -1;
  Token() = default;
  Token(TokenType _token_type, StringRef _lexeme, std::size_t _row, std::size_t _col)
    : type(_token_type), lexeme(_lexeme), row(_row), col(_col) {}

  friend std::ostream& operator<<(std::ostream &os, const Token &token) {
    os << token.row << ' ' << token.col << ' '
       << token_type_to_string(token.type) << ' '
       << token.lexeme;
    return os;
  }
};

class Lexer {
  StringRef _src_code;
  std::vector<Token> _tokens;
  std::size_t _pos = 0; // points to first unhandled character
  std::size_t _row = 1, _col = 1; // exact position of _pos
  enum class Error : uint8_t {
    kSuccess, kWhitespaceComment, kNumber, kCharString, kKeywordIdentifier, kPunctuationDelimiter,
  };
  Error _error_code = Error::kSuccess;

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
  explicit Lexer(StringRef code) { tokenize(code); }
  // return false if tokenization fails (lexical/syntax error)
  void tokenize(StringRef code);

  explicit operator bool() const { return _error_code == Error::kSuccess; }
  [[nodiscard]] bool is_good() const { return _error_code == Error::kSuccess; }
  [[nodiscard]] const std::vector<Token>& tokens() const { return _tokens; }
  [[nodiscard]] std::vector<Token> release() {
    _error_code = Error::kSuccess;
    _pos = 0; _row = 1; _col = 1;
    return std::move(_tokens);
  }
  [[nodiscard]] std::string error_msg() const {
    std::string spec_msg;
    switch(_error_code) {
    case Error::kSuccess: return "Compilation succeeded / not started";
    case Error::kKeywordIdentifier: spec_msg = "Keyword/Identifier tokenization failure.";
      break;
    case Error::kNumber: spec_msg = "Number resolution failure.";
      break;
    case Error::kCharString: spec_msg = "Character/String resolution failure.";
      break;
    case Error::kWhitespaceComment: spec_msg = "Whitespace/Comment resolution failure.";
      break;
    case Error::kPunctuationDelimiter: spec_msg = "Punctuation/Delimiter resolution failure.";
      break;
    default: throw std::runtime_error("Lexer: Unexpected error type.");
    }
    std::string common_msg = "Compile error(lexer): Something unresolvable emerges at " +
      std::to_string(_row) + ':' + std::to_string(_col);
    return common_msg + ": " + spec_msg;
  }
};

}

#endif // INSOMNIA_LEXER_H