#include "parser.h"

namespace insomnia::rust_shard::ast {

class Parser::Context {
  friend Backtracker;
  std::vector<Token> _tokens;
  std::size_t _pos = 0;
public:
  Context() = default;
  template <class T>
  explicit Context(T &&tokens, std::size_t pos = 0)
    : _tokens(std::forward<T>(tokens)), _pos(pos) {}
  // returns a default token with token_type == INVALID if fails.
  [[nodiscard]] Token peek(std::size_t diff = 1) const {
    if(_pos + diff >= _tokens.size())
      return Token{};
    return _tokens[_pos + diff];
  }
  // returns a default token with token_type == INVALID if fails.
  [[nodiscard]] Token current() const {
    if(_pos >= _tokens.size())
      return Token{};
    return _tokens[_pos];
  }
  void consume() {
    if(_pos >= _tokens.size())
      throw std::runtime_error("ASTContext consume out of range.");
    _pos++;
  }
  [[nodiscard]] bool is_peek_safe(std::size_t diff = 1) const { return _pos + diff < _tokens.size(); }
  [[nodiscard]] bool empty() const { return _pos >= _tokens.size(); }
  void reset() { _pos = 0; }
};

class Parser::Backtracker {
  Context &_ast_ctx;
  std::size_t _pos;
  bool _commited;
public:
  explicit Backtracker(Context &ast_ctx)
  : _ast_ctx(ast_ctx), _pos(ast_ctx._pos), _commited(false) {}
  ~Backtracker() {
    if(!_commited) _ast_ctx._pos = _pos;
  }
  void commit() { _commited = true; }
};

void Parser::parseALl(Lexer &lexer) {
  _ast_ctx = std::make_unique<Context>(lexer.release());
  _crate = parse_crate();
  _is_good = static_cast<bool>(_crate);
}

}