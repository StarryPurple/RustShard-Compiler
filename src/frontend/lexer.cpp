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

  _error_code = Error::SUCCESS;
  while(true) {
    if(!advance()) {
      _error_code = Error::WHITESPACE_COMMENT; break;
    }
    if(_pos >= src_len) break;
    const char &ch = code[_pos];
    if(is_digit(ch)) {
      // Number
      if(!tokenize_number_literal()) {
        _error_code = Error::NUMBER; break;
      }
    } else if(ch == '\'' || ch == '\"') { // TODO: C / Raw String
      // Character / String
      if(!tokenize_string_literal()) {
        _error_code = Error::STRING; break;
      }
    } else if(is_alpha(ch) ||
      (ch == '_' && _pos + 1 < src_len &&
        (is_alpha(code[_pos + 1]) || is_digit(code[_pos + 1]) || code[_pos + 1] == '_'))) {
      // Keyword / Identifier
      if(!tokenize_keyword_identifier()) { // NOLINT. I know this won't fail.
        _error_code = Error::KW_ID; break;
      }
    } else {
      // Punctuation / Delimiter
      if(!tokenize_punctuation_delimiter()) {
        _error_code = Error::PUNC_DELIM; break;
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
          _tokens.emplace_back(TokenType::FLOAT_LITERAL, matched, _row, _col);
          _pos += matched.length(); _col += matched.length();
          return true;
        }
        // might be an operator dot / dot_dot / ...
        // ignore. No suffix starts with a dot, so return now.
        _tokens.emplace_back(TokenType::INTEGER_LITERAL, matched, _row, _col);
        _pos += matched.length(); _col += matched.length();
        return true;
      }
    }
    std::string_view remain = str.substr(match.length());
    if(std::regex_search(remain.begin(), remain.end(), match, integer_suffix_pattern)) {
      matched = _src_code.substr(_pos, matched.length() + match.length());
    }
    _tokens.emplace_back(TokenType::INTEGER_LITERAL, matched, _row, _col);
    _pos += matched.length(); _col += matched.length();
    return true;
  }
  return false;
}

bool is_escape(char ch) { return ch == '\n' || ch == '\r' || ch == '\t' || ch == '\\' || ch == '\0'; }

bool Lexer::tokenize_string_literal() {
  auto start = _pos, old_row = _row, old_col = _col;
  char c = _src_code[_pos]; advance_one();
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
      _tokens.emplace_back(TokenType::STRING_LITERAL, _src_code.substr(start, _pos - start), _row, _col);
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
  TokenType type = TokenType::INVALID;
  auto len = _src_code.length();
  switch(ch) {
  case '+': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::PLUS_EQ;
      advance_one(); advance_one();
    } else {
      type = TokenType::PLUS;
      advance_one();
    }
  } break;
  case '-': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::MINUS_EQ;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '>') {
      type = TokenType::R_ARROW;
      advance_one(); advance_one();
    } else {
      type = TokenType::MINUS;
      advance_one();
    }
  } break;
  case '*': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::STAR_EQ;
      advance_one(); advance_one();
    } else {
      type = TokenType::STAR;
      advance_one();
    }
  } break;
  case '/': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::SLASH_EQ;
      advance_one(); advance_one();
    } else {
      type = TokenType::SLASH;
      advance_one();
    }
  } break;
  case '%': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::PERCENT_EQ;
      advance_one(); advance_one();
    } else {
      type = TokenType::PERCENT;
      advance_one();
    }
  } break;
  case '^': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::CARET_EQ;
      advance_one(); advance_one();
    } else {
      type = TokenType::CARET;
      advance_one();
    }
  } break;
  case '!': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::NE;
      advance_one(); advance_one();
    } else {
      type = TokenType::NOT;
      advance_one();
    }
  } break;
  case '&': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::AND_EQ;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '&') {
      type = TokenType::AND_AND;
      advance_one(); advance_one();
    } else {
      type = TokenType::AND;
      advance_one();
    }
  } break;
  case '|': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::OR_EQ;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '|') {
      type = TokenType::OR_OR;
      advance_one(); advance_one();
    } else {
      type = TokenType::OR;
      advance_one();
    }
  } break;
  case '<': {
    if(_pos + 2 < len && _src_code[_pos + 1] == '<' && _src_code[_pos + 2] == '=') {
      type = TokenType::SHL_EQ;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '<') {
      type = TokenType::SHL;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::LE;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '-') {
      type = TokenType::L_ARROW;
      advance_one(); advance_one();
    } else {
      type = TokenType::LT;
      advance_one();
    }
  } break;
  case '>': {
    if(_pos + 2 < len && _src_code[_pos + 1] == '>' && _src_code[_pos + 2] == '=') {
      type = TokenType::SHR_EQ;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '>') {
      type = TokenType::SHR;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::GE;
      advance_one(); advance_one();
    } else {
      type = TokenType::GT;
      advance_one();
    }
  } break;
  case '=': {
    if(_pos + 1 < len && _src_code[_pos + 1] == '=') {
      type = TokenType::EQ_EQ;
      advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '>') {
      type = TokenType::FAT_ARROW;
      advance_one(); advance_one();
    } else {
      type = TokenType::EQ;
      advance_one();
    }
  } break;
  case '.': {
    if(_pos + 2 < len && _src_code[_pos + 1] == '.' && _src_code[_pos + 2] == '.') {
      type = TokenType::DOT_DOT_DOT;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 2 < len && _src_code[_pos + 1] == '.' && _src_code[_pos + 2] == '=') {
      type = TokenType::DOT_DOT_EQ;
      advance_one(); advance_one(); advance_one();
    } else if(_pos + 1 < len && _src_code[_pos + 1] == '.') {
      type = TokenType::DOT_DOT;
      advance_one(); advance_one();
    } else {
      type = TokenType::DOT;
      advance_one();
    }
  } break;
  case ':': {
    if(_pos + 1 < len && _src_code[_pos + 1] == ':') {
      type = TokenType::PATH_SEP;
      advance_one(); advance_one();
    } else {
      type = TokenType::COLON;
      advance_one();
    }
  } break;
  case '#': type = TokenType::POUND; advance_one(); break;
  case '$': type = TokenType::DOLLAR; advance_one(); break;
  case '?': type = TokenType::QUESTION; advance_one(); break;
  case '~': type = TokenType::TILDE; advance_one(); break;
  case '@': type = TokenType::AT; advance_one(); break;
  case ',': type = TokenType::COMMA; advance_one(); break;
  case ';': type = TokenType::SEMI; advance_one(); break;
  case '_': type = TokenType::UNDERSCORE; advance_one(); break;
  case '(': type = TokenType::L_PARENTHESIS; advance_one(); break;
  case ')': type = TokenType::R_PARENTHESIS; advance_one(); break;
  case '[': type = TokenType::L_SQUARE_BRACKET; advance_one(); break;
  case ']': type = TokenType::R_SQUARE_BRACKET; advance_one(); break;
  case '{': type = TokenType::L_CURLY_BRACE; advance_one(); break;
  case '}': type = TokenType::R_CURLY_BRACE; advance_one(); break;
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
    {"as", TokenType::AS}, {"break", TokenType::BREAK}, {"const", TokenType::CONST},
    {"continue", TokenType::CONTINUE}, {"crate", TokenType::CRATE}, {"else", TokenType::ELSE},
    {"enum", TokenType::ENUM}, {"extern", TokenType::EXTERN}, {"false", TokenType::FALSE},
    {"fn", TokenType::FN}, {"for", TokenType::FOR}, {"if", TokenType::IF},
    {"impl", TokenType::IMPL}, {"in", TokenType::IN}, {"let", TokenType::LET},
    {"loop", TokenType::LOOP}, {"match", TokenType::MATCH}, {"mod", TokenType::MOD},
    {"move", TokenType::MOVE}, {"mut", TokenType::MUT}, {"pub", TokenType::PUB},
    {"ref", TokenType::REF}, {"return", TokenType::RETURN}, {"self", TokenType::SELF_OBJECT},
    {"Self", TokenType::SELF_TYPE}, {"static", TokenType::STATIC}, {"struct", TokenType::STRUCT},
    {"super", TokenType::SUPER}, {"trait", TokenType::TRAIT}, {"true", TokenType::TRUE},
    {"type", TokenType::TYPE}, {"unsafe", TokenType::UNSAFE}, {"use", TokenType::USE},
    {"where", TokenType::WHERE}, {"while", TokenType::WHILE}, {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT}, {"dyn", TokenType::DYN},
  };
  if(auto it = keywords.find(lexeme); it != keywords.end()) {
    _tokens.emplace_back(it->second, lexeme, old_row, old_col);
  } else {
    _tokens.emplace_back(TokenType::IDENTIFIER, lexeme, old_row, old_col);
  }
  return true;
}

std::string_view token_type_to_string(TokenType type) {
  static const std::unordered_map<TokenType, std::string_view> token_names = {
    {TokenType::INVALID, "INVALID"}, {TokenType::UNKNOWN, "UNKNOWN"}, {TokenType::END_OF_FILE, "EOF"},
    {TokenType::AS, "AS"}, {TokenType::BREAK, "BREAK"}, {TokenType::CONST, "CONST"},
    {TokenType::CONTINUE, "CONTINUE"}, {TokenType::CRATE, "CRATE"}, {TokenType::ELSE, "ELSE"},
    {TokenType::ENUM, "ENUM"}, {TokenType::EXTERN, "EXTERN"}, {TokenType::FALSE, "FALSE"},
    {TokenType::FN, "FN"}, {TokenType::FOR, "FOR"}, {TokenType::IF, "IF"},
    {TokenType::IMPL, "IMPL"}, {TokenType::IN, "IN"}, {TokenType::LET, "LET"},
    {TokenType::LOOP, "LOOP"}, {TokenType::MATCH, "MATCH"}, {TokenType::MOD, "MOD"},
    {TokenType::MOVE, "MOVE"}, {TokenType::MUT, "MUT"}, {TokenType::PUB, "PUB"},
    {TokenType::REF, "REF"}, {TokenType::RETURN, "RETURN"}, {TokenType::SELF_OBJECT, "SELF_OBJECT"},
    {TokenType::SELF_TYPE, "SELF_TYPE"}, {TokenType::STATIC, "STATIC"}, {TokenType::STRUCT, "STRUCT"},
    {TokenType::SUPER, "SUPER"}, {TokenType::TRAIT, "TRAIT"}, {TokenType::TRUE, "TRUE"},
    {TokenType::TYPE, "TYPE"}, {TokenType::UNSAFE, "UNSAFE"}, {TokenType::USE, "USE"},
    {TokenType::WHERE, "WHERE"}, {TokenType::WHILE, "WHILE"}, {TokenType::ASYNC, "ASYNC"},
    {TokenType::AWAIT, "AWAIT"}, {TokenType::DYN, "DYN"},
    {TokenType::IDENTIFIER, "IDENTIFIER"},
    {TokenType::LITERAL, "LITERAL"}, {TokenType::INTEGER_LITERAL, "INTEGER_LITERAL"},
    {TokenType::FLOAT_LITERAL, "FLOAT_LITERAL"}, {TokenType::STRING_LITERAL, "STRING_LITERAL"},
    {TokenType::PLUS, "PLUS"}, {TokenType::MINUS, "MINUS"}, {TokenType::STAR, "STAR"},
    {TokenType::SLASH, "SLASH"}, {TokenType::PERCENT, "PERCENT"}, {TokenType::CARET, "CARET"},
    {TokenType::NOT, "NOT"}, {TokenType::AND, "AND"}, {TokenType::OR, "OR"},
    {TokenType::AND_AND, "AND_AND"}, {TokenType::OR_OR, "OR_OR"}, {TokenType::SHL, "SHL"},
    {TokenType::SHR, "SHR"}, {TokenType::PLUS_EQ, "PLUS_EQ"}, {TokenType::MINUS_EQ, "MINUS_EQ"},
    {TokenType::STAR_EQ, "STAR_EQ"}, {TokenType::SLASH_EQ, "SLASH_EQ"}, {TokenType::PERCENT_EQ, "PERCENT_EQ"},
    {TokenType::CARET_EQ, "CARET_EQ"}, {TokenType::AND_EQ, "AND_EQ"}, {TokenType::OR_EQ, "OR_EQ"},
    {TokenType::SHL_EQ, "SHL_EQ"}, {TokenType::SHR_EQ, "SHR_EQ"}, {TokenType::EQ, "EQ"},
    {TokenType::EQ_EQ, "EQ_EQ"}, {TokenType::NE, "NE"}, {TokenType::GT, "GT"},
    {TokenType::LT, "LT"}, {TokenType::GE, "GE"}, {TokenType::LE, "LE"}, {TokenType::AT, "AT"},
    {TokenType::UNDERSCORE, "UNDERSCORE"}, {TokenType::DOT, "DOT"}, {TokenType::DOT_DOT, "DOT_DOT"},
    {TokenType::DOT_DOT_DOT, "DOT_DOT_DOT"}, {TokenType::DOT_DOT_EQ, "DOT_DOT_EQ"},
    {TokenType::COMMA, "COMMA"}, {TokenType::SEMI, "SEMI"}, {TokenType::COLON, "COLON"},
    {TokenType::PATH_SEP, "PATH_SEP"}, {TokenType::R_ARROW, "R_ARROW"}, {TokenType::FAT_ARROW, "FAT_ARROW"},
    {TokenType::L_ARROW, "L_ARROW"}, {TokenType::POUND, "POUND"}, {TokenType::DOLLAR, "DOLLAR"},
    {TokenType::QUESTION, "QUESTION"}, {TokenType::TILDE, "TILDE"},
    {TokenType::L_CURLY_BRACE, "L_CURLY_BRACE"}, {TokenType::R_CURLY_BRACE, "R_CURLY_BRACE"},
    {TokenType::L_SQUARE_BRACKET, "L_SQUARE_BRACKET"}, {TokenType::R_SQUARE_BRACKET, "R_SQUARE_BRACKET"},
    {TokenType::L_PARENTHESIS, "L_PARENTHESIS"}, {TokenType::R_PARENTHESIS, "R_PARENTHESIS"},
  };
  if(const auto it = token_names.find(type); it != token_names.end()) {
    return it->second;
  }
  return "UNKNOWN_TOKEN";
}

}
