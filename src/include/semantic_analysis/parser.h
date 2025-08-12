#ifndef INSOMNIA_PARSER_H
#define INSOMNIA_PARSER_H
#include "ast.h"
#include "lexer.h"

namespace insomnia {

class Parser {

  // Token flow
  class ASTContext {
    std::vector<Token> _tokens;
    std::size_t _pos = 0;
  public:
    ASTContext() = default;
    explicit ASTContext(std::vector<Token> tokens, std::size_t pos = 0)
      : _tokens(std::move(tokens)), _pos(pos) {}

    Token peek() const {
      if(_pos + 1 >= _tokens.size())
        throw std::runtime_error("ASTContext peek out of range.");
      return _tokens[_pos + 1];
    }
    Token consume() {
      if(_pos >= _tokens.size())
        throw std::runtime_error("ASTContext consume out of range.");
      return _tokens[_pos++];
    }
    bool empty() const { return _pos >= _tokens.size(); }
    void reset() { _pos = 0; }
  };
  ASTContext _ast_ctx;
  bool _is_good;

  std::shared_ptr<ASTCrate> _crate;

  std::unique_ptr<ASTCrate> parse_crate();
  std::unique_ptr<ASTItem> parse_item();
  std::unique_ptr<ASTVisItem> parse_vis_item();
  std::unique_ptr<ASTFunction> parse_function();
  std::unique_ptr<ASTStruct> pares_struct();
  std::unique_ptr<ASTEnumeration> parse_enumeration();
  std::unique_ptr<ASTConstantItem> parse_constant_item();
  std::unique_ptr<ASTImplementation> parse_implementation();

public:
  Parser();
  explicit Parser(const Lexer &lexer) : _ast_ctx(lexer.tokens()) {
    _crate = parse_crate();
  }

  void parse(const Lexer &lexer) {
    _ast_ctx = ASTContext(lexer.tokens());
    _crate = parse_crate();
  }

  [[nodiscard]] explicit operator bool() const { return _is_good; }
  [[nodiscard]] bool is_good() const { return _is_good; }
};

}

#endif // INSOMNIA_PARSER_H