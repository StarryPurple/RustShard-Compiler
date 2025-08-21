#include "lexer.h"

#include <regex>
#include <unordered_map>

namespace insomnia::rust_shard {

bool is_whitespace(char ch) { return ch == '\t' || ch == '\n' || ch == '\r' || ch == ' '; }
bool is_alpha(char ch) { return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z'); }
bool is_digit(char ch) { return '0' <= ch && ch <= '9'; }

void Lexer::tokenize(std::string_view code) {
  _src_code = code;
  _tokens.clear();
  _pos = 0; _row = 1; _col = 1;
  std::size_t src_len = code.length();

  _error_code = Error::kSuccess;
  while(true) {
    if(!advance()) {
      _error_code = Error::kWhitespaceComment; break;
    }
    if(_pos >= src_len) break;
    const char &ch = code[_pos];
    if(is_digit(ch)) {
      // Number
      if(!tokenize_number_literal()) {
        _error_code = Error::kNumber; break;
      }
    } else if(ch == '\'' || ch == '\"') { // TODO: C / Raw String
      // Character / String
      if(!tokenize_string_literal()) {
        _error_code = Error::kCharString; break;
      }
    } else if(is_alpha(ch) ||
      (ch == '_' && _pos + 1 < src_len &&
        (is_alpha(code[_pos + 1]) || is_digit(code[_pos + 1]) || code[_pos + 1] == '_'))) {
      // Keyword / Identifier
      if(!tokenize_keyword_identifier()) { // NOLINT. I know this won't fail.
        _error_code = Error::kKeywordIdentifier; break;
      }
    } else {
      // Punctuation / Delimiter
      if(!tokenize_punctuation_delimiter()) {
        _error_code = Error::kPunctuationDelimiter; break;
      }
    }
  }
}

bool Lexer::advance() {
  auto code_len = _src_code.length();
  auto start = _pos, old_row = _row, old_col = _col;
  while(_pos < code_len) {
    // single line comment
    if(_src_code[_pos] == '/' && _pos + 1 < code_len && _src_code[_pos + 1] == '/') {
      advance_one(); advance_one();
      while(_pos < code_len && _src_code[_pos] != '\n') {
        advance_one();
      }
      // points to EOF or '\n' here
      continue;
    }
    // multiple line comment
    if(_src_code[_pos] == '/' && _pos + 1 < code_len && _src_code[_pos + 1] == '*') {
      advance_one(); advance_one();
      std::size_t cnt = 1;
      // not pointing to the first '*'
      while(_pos + 1 < code_len && cnt > 0) {
        if(_src_code[_pos] == '*' && _src_code[_pos + 1] == '/') {
          advance_one(); advance_one();
          cnt--;
        } else if(_src_code[_pos] == '/' && _src_code[_pos + 1] == '*') {
          advance_one(); advance_one();
          cnt++;
        } else {
          advance_one();
        }
      }
      // points to EOF or the character after "*/" here
      if(cnt != 0) {
        _pos = start; _row = old_row; _col = old_col;
        return false;
      }
      continue;
    }
    // other whitespaces
    if(!is_whitespace(_src_code[_pos])) break;
    advance_one();
  }
  return true;
}


bool Lexer::tokenize_number_literal() {
  static const std::regex hex_pattern(R"(^0x[\da-fA-F_]*[\da-fA-F][\da-fA-F_]*)");
  static const std::regex oct_pattern("^0o[0-7_]*[0-7][0-7_]*");
  static const std::regex bin_pattern("^0b[0-1_]*[0-1][0-1_]*");
  static const std::regex dec_pattern(R"(^[\d][\d_]*)");
  static const std::regex integer_suffix_pattern("^(u|i)(8|16|32|64|size)");
  static const std::regex float_pattern(R"(^[\d][\d_]*(\.[^\w\.]|\.[\d][\d_]*|(\.[\d][\d_]*)?(f32|f64)))");
  std::string_view str = _src_code.substr(_pos);
  std::cmatch match;
  if(std::regex_search(str.begin(), str.end(), match, hex_pattern) ||
    std::regex_search(str.begin(), str.end(), match, oct_pattern) ||
    std::regex_search(str.begin(), str.end(), match, bin_pattern) ||
    std::regex_search(str.begin(), str.end(), match, dec_pattern)) {
    // number matched. now match suffix
    std::string_view matched = str.substr(0, match.length());
    auto nxt_pos = _pos + match.length();
    if(nxt_pos < _src_code.length()) {
      char nxt_ch  = _src_code[nxt_pos];
      if(nxt_ch == '.') {
        if(std::regex_search(str.begin(), str.end(), match, float_pattern)) {
          matched = str.substr(0, match.length());
          _tokens.emplace_back(TokenType::kFloatLiteral, matched, _row, _col);
          _pos += matched.length(); _col += matched.length();
          return true;
        }
        // might be an operator dot / dot_dot / ...
        // ignore. No suffix starts with a dot, so return now.
        _tokens.emplace_back(TokenType::kIntegerLiteral, matched, _row, _col);
        _pos += matched.length(); _col += matched.length();
        return true;
      }
    }
    std::string_view remain = str.substr(match.length());
    if(std::regex_search(remain.begin(), remain.end(), match, integer_suffix_pattern)) {
      matched = _src_code.substr(_pos, matched.length() + match.length());
    }
    _tokens.emplace_back(TokenType::kIntegerLiteral, matched, _row, _col);
    _pos += matched.length(); _col += matched.length();
    return true;
  }
  return false;
}

bool is_escape(char ch) { return ch == '\n' || ch == '\r' || ch == '\t' || ch == '\\' || ch == '\0'; }

bool Lexer::tokenize_string_literal() {
  auto start = _pos, old_row = _row, old_col = _col;
  char c = _src_code[_pos];
  TokenType tp = (c == '\"' ? TokenType::kStringLiteral : TokenType::kCharLiteral);
  advance_one();
  while(_pos < _src_code.length()) {
    if(_src_code[_pos] == '\\') {
      if(_pos + 2 >= _src_code.length()) {
        _pos = start; _row = old_row; _col = old_col;
        return false;
      }
      advance_one(); advance_one();
      continue;
    }
    if(_src_code[_pos] == c) {
      advance_one();
      if(c == '\'' && _pos - start != 2) return false; // Not a valid char
      _tokens.emplace_back(tp, _src_code.substr(start, _pos - start), _row, _col);
      return true;
    }
    advance_one();
  }
  _pos = start; _row = old_row; _col = old_col;
  return false;
}

bool Lexer::tokenize_punctuation_delimiter() {
  auto start = _pos; char ch = _src_code[_pos];
  auto old_row = _row, old_col = _col;
  TokenType type = TokenType::kInvalid;
  auto len = _src_code.length();
  switch(ch) {
  case '+': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kPlusEq;
      advance_one(); advance_one();
    } else {
      type = TokenType::kPlus;
      advance_one();
    }
  } break;
  case '-': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kMinusEq;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '>') {
      type = TokenType::kRArrow;
      advance_one(); advance_one();
    } else {
      type = TokenType::kMinus;
      advance_one();
    }
  } break;
  case '*': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kStarEq;
      advance_one(); advance_one();
    } else {
      type = TokenType::kStar;
      advance_one();
    }
  } break;
  case '/': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kSlashEq;
      advance_one(); advance_one();
    } else {
      type = TokenType::kSlash;
      advance_one();
    }
  } break;
  case '%': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kPercentEq;
      advance_one(); advance_one();
    } else {
      type = TokenType::kPercent;
      advance_one();
    }
  } break;
  case '^': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kCaretEq;
      advance_one(); advance_one();
    } else {
      type = TokenType::kCaret;
      advance_one();
    }
  } break;
  case '!': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kNe;
      advance_one(); advance_one();
    } else {
      type = TokenType::kNot;
      advance_one();
    }
  } break;
  case '&': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kAndEq;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '&') {
      type = TokenType::kAndAnd;
      advance_one(); advance_one();
    } else {
      type = TokenType::kAnd;
      advance_one();
    }
  } break;
  case '|': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kOrEq;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '|') {
      type = TokenType::kOrOr;
      advance_one(); advance_one();
    } else {
      type = TokenType::kOr;
      advance_one();
    }
  } break;
  case '<': {
    if(_pos + 2 < len && _src_code[_pos + 1] == '<' && _src_code[_pos + 2] == '=') {
      type = TokenType::kShlEq;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '<') {
      type = TokenType::kShl;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kLe;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '-') {
      type = TokenType::kLArrow;
      advance_one(); advance_one();
    } else {
      type = TokenType::kLt;
      advance_one();
    }
  } break;
  case '>': {
    if(_pos + 2 < len && _src_code[_pos + 1] == '>' && _src_code[_pos + 2] == '=') {
      type = TokenType::kShrEq;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '>') {
      type = TokenType::kShr;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kGe;
      advance_one(); advance_one();
    } else {
      type = TokenType::kGt;
      advance_one();
    }
  } break;
  case '=': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::kEqEq;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '>') {
      type = TokenType::kFatArrow;
      advance_one(); advance_one();
    } else {
      type = TokenType::kEq;
      advance_one();
    }
  } break;
  case '.': {
    if(_pos + 2 < len && _src_code[_pos + 1] == '.' && _src_code[_pos + 2] == '.') {
      type = TokenType::kDotDotDot;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 2 < len && _src_code[_pos + 1] == '.' && _src_code[_pos + 2] == '=') {
      type = TokenType::kDotDotEq;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '.') {
      type = TokenType::kDotDot;
      advance_one(); advance_one();
    } else {
      type = TokenType::kDot;
      advance_one();
    }
  } break;
  case ':': {
    if(_pos + 1 < len && _src_code[_pos + 1] == ':') {
      type = TokenType::kPathSep;
      advance_one(); advance_one();
    } else {
      type = TokenType::kColon;
      advance_one();
    }
  } break;
  case '#': type = TokenType::kPound; advance_one(); break;
  case '$': type = TokenType::kDollar; advance_one(); break;
  case '?': type = TokenType::kQuestion; advance_one(); break;
  case '~': type = TokenType::kTilde; advance_one(); break;
  case '@': type = TokenType::kAt; advance_one(); break;
  case ',': type = TokenType::kComma; advance_one(); break;
  case ';': type = TokenType::kSemi; advance_one(); break;
  case '_': type = TokenType::kUnderscore; advance_one(); break;
  case '(': type = TokenType::kLParenthesis; advance_one(); break;
  case ')': type = TokenType::kRParenthesis; advance_one(); break;
  case '[': type = TokenType::kLSquareBracket; advance_one(); break;
  case ']': type = TokenType::kRSquareBracket; advance_one(); break;
  case '{': type = TokenType::kLCurlyBrace; advance_one(); break;
  case '}': type = TokenType::kRCurlyBrace; advance_one(); break;
  default: _pos = start; _row = old_row; _col = old_col; return false;
  }
  _tokens.emplace_back(type, _src_code.substr(start, _pos - start), old_row, old_col);
  return true;
}


bool Lexer::tokenize_keyword_identifier() {
  auto start = _pos, old_row = _row, old_col = _col; advance_one();
  auto len = _src_code.length();
  while(_pos < len && (is_alpha(_src_code[_pos]) || is_digit(_src_code[_pos]) || _src_code[_pos] == '_')) {
    advance_one();
  }
  std::string_view lexeme = _src_code.substr(start, _pos - start);
  static const std::unordered_map<std::string_view, TokenType> keywords = {
    {"as", TokenType::kAs}, {"break", TokenType::kBreak}, {"const", TokenType::kConst},
    {"continue", TokenType::kContinue}, {"crate", TokenType::kCrate}, {"else", TokenType::kElse},
    {"enum", TokenType::kEnum}, {"extern", TokenType::kExtern}, {"false", TokenType::kFalse},
    {"fn", TokenType::kFn}, {"for", TokenType::kFor}, {"if", TokenType::kIf},
    {"impl", TokenType::kImpl}, {"in", TokenType::kIn}, {"let", TokenType::kLet},
    {"loop", TokenType::kLoop}, {"match", TokenType::kMatch}, {"mod", TokenType::kMod},
    {"move", TokenType::kMove}, {"mut", TokenType::kMut}, {"pub", TokenType::kPub},
    {"ref", TokenType::kRef}, {"return", TokenType::kReturn}, {"self", TokenType::kSelfObject},
    {"Self", TokenType::kSelfType}, {"static", TokenType::kStatic}, {"struct", TokenType::kStruct},
    {"super", TokenType::kSuper}, {"trait", TokenType::kTrait}, {"true", TokenType::kTrue},
    {"type", TokenType::kType}, {"unsafe", TokenType::kUnsafe}, {"use", TokenType::kUse},
    {"where", TokenType::kWhere}, {"while", TokenType::kWhile}, {"async", TokenType::kAsync},
    {"await", TokenType::kAwait}, {"dyn", TokenType::kDyn},
  };
  if(auto it = keywords.find(lexeme); it != keywords.end()) {
    _tokens.emplace_back(it->second, lexeme, old_row, old_col);
  } else {
    _tokens.emplace_back(TokenType::kIdentifier, lexeme, old_row, old_col);
  }
  return true;
}

std::string token_type_to_string(TokenType type) {
  static const std::unordered_map<TokenType, std::string> token_names = {
    {TokenType::kInvalid, "kInvalid"}, {TokenType::kUnknown, "kUnknown"}, {TokenType::kEndOfFile, "kEndOfFile"},
    {TokenType::kAs, "kAs"}, {TokenType::kBreak, "kBreak"}, {TokenType::kConst, "kConst"},
    {TokenType::kContinue, "kContinue"}, {TokenType::kCrate, "kCrate"}, {TokenType::kElse, "kElse"},
    {TokenType::kEnum, "kEnum"}, {TokenType::kExtern, "kExtern"}, {TokenType::kFalse, "kFalse"},
    {TokenType::kFn, "kFn"}, {TokenType::kFor, "kFor"}, {TokenType::kIf, "kIf"},
    {TokenType::kImpl, "kImpl"}, {TokenType::kIn, "kIn"}, {TokenType::kLet, "kLet"},
    {TokenType::kLoop, "kLoop"}, {TokenType::kMatch, "kMatch"}, {TokenType::kMod, "kMod"},
    {TokenType::kMove, "kMove"}, {TokenType::kMut, "kMut"}, {TokenType::kPub, "kPub"},
    {TokenType::kRef, "kRef"}, {TokenType::kReturn, "kReturn"}, {TokenType::kSelfObject, "kSelfObject"},
    {TokenType::kSelfType, "kSelfType"}, {TokenType::kStatic, "kStatic"}, {TokenType::kStruct, "kStruct"},
    {TokenType::kSuper, "kSuper"}, {TokenType::kTrait, "kTrait"}, {TokenType::kTrue, "kTrue"},
    {TokenType::kType, "kType"}, {TokenType::kUnsafe, "kUnsafe"}, {TokenType::kUse, "kUse"},
    {TokenType::kWhere, "kWhere"}, {TokenType::kWhile, "kWhile"}, {TokenType::kAsync, "kAsync"},
    {TokenType::kAwait, "kAwait"}, {TokenType::kDyn, "kDyn"},
    {TokenType::kIdentifier, "kIdentifier"},
    {TokenType::kLiteral, "kLiteral"}, {TokenType::kIntegerLiteral, "kIntegerLiteral"},
    {TokenType::kFloatLiteral, "kFloatLiteral"}, {TokenType::kStringLiteral, "kStringLiteral"},
    {TokenType::kPlus, "kPlus"}, {TokenType::kMinus, "kMinus"}, {TokenType::kStar, "kStar"},
    {TokenType::kSlash, "kSlash"}, {TokenType::kPercent, "kPercent"}, {TokenType::kCaret, "kCaret"},
    {TokenType::kNot, "kNot"}, {TokenType::kAnd, "kAnd"}, {TokenType::kOr, "kOr"},
    {TokenType::kAndAnd, "kAndAnd"}, {TokenType::kOrOr, "kOrOr"}, {TokenType::kShl, "kShl"},
    {TokenType::kShr, "kShr"}, {TokenType::kPlusEq, "kPlusEq"}, {TokenType::kMinusEq, "kMinusEq"},
    {TokenType::kStarEq, "kStarEq"}, {TokenType::kSlashEq, "kSlashEq"}, {TokenType::kPercentEq, "kPercentEq"},
    {TokenType::kCaretEq, "kCaretEq"}, {TokenType::kAndEq, "kAndEq"}, {TokenType::kOrEq, "kOrEq"},
    {TokenType::kShlEq, "kShlEq"}, {TokenType::kShrEq, "kShrEq"}, {TokenType::kEq, "kEq"},
    {TokenType::kEqEq, "kEqEq"}, {TokenType::kNe, "kNe"}, {TokenType::kGt, "kGt"},
    {TokenType::kLt, "kLt"}, {TokenType::kGe, "kGe"}, {TokenType::kLe, "kLe"}, {TokenType::kAt, "kAt"},
    {TokenType::kUnderscore, "kUnderscore"}, {TokenType::kDot, "kDot"}, {TokenType::kDotDot, "kDotDot"},
    {TokenType::kDotDotDot, "kDotDotDot"}, {TokenType::kDotDotEq, "kDotDotEq"},
    {TokenType::kComma, "kComma"}, {TokenType::kSemi, "kSemi"}, {TokenType::kColon, "kColon"},
    {TokenType::kPathSep, "kPathSep"}, {TokenType::kRArrow, "kRArrow"}, {TokenType::kFatArrow, "kFatArrow"},
    {TokenType::kLArrow, "kLArrow"}, {TokenType::kPound, "kPound"}, {TokenType::kDollar, "kDollar"},
    {TokenType::kQuestion, "kQuestion"}, {TokenType::kTilde, "kTilde"},
    {TokenType::kLCurlyBrace, "kLCurlyBrace"}, {TokenType::kRCurlyBrace, "kRCurlyBrace"},
    {TokenType::kLSquareBracket, "kLSquareBracket"}, {TokenType::kRSquareBracket, "kRSquareBracket"},
    {TokenType::kLParenthesis, "kLParenthesis"}, {TokenType::kRParenthesis, "kRParenthesis"},
  };
  if(const auto it = token_names.find(type); it != token_names.end()) {
    return it->second;
  }
  return "UnknownToken";
}

}