#include "frontend/parser.h"

namespace insomnia::ast {

class Parser::Context {
  friend Backtracker;
  std::vector<Token> _tokens;
  std::size_t _pos = 0;
public:
  Context() = default;
  template <class T>
  explicit Context(T &&tokens, std::size_t pos = 0)
    : _tokens(std::forward<T>(tokens)), _pos(pos) {}

  const Token& peek(std::size_t diff = 1) const {
    if(_pos + diff >= _tokens.size())
      throw std::runtime_error("ASTContext peek out of range.");
    return _tokens[_pos + diff];
  }
  const Token& current() const {
    if(_pos >= _tokens.size())
      throw std::runtime_error("ASTContext current out of range.");
    return _tokens[_pos];
  }
  void consume() {
    if(_pos >= _tokens.size())
      throw std::runtime_error("ASTContext consume out of range.");
    _pos++;
  }
  bool is_peek_safe(std::size_t diff = 1) const {
    return _pos + diff < _tokens.size();
  }
  bool empty() const { return _pos >= _tokens.size(); }
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

void Parser::parse(Lexer &lexer) {
  _ast_ctx = std::make_unique<Context>(lexer.release());
  _crate = parse_crate();
  _is_good = static_cast<bool>(_crate);
}

std::unique_ptr<Crate> Parser::parse_crate() {
  std::vector<std::unique_ptr<Item>> vec;
  while(!_ast_ctx->empty()) {
    auto item = parse_item();
    if(!item) return nullptr;
    vec.push_back(std::move(item));
  }
  return std::make_unique<Crate>(std::move(vec));
}

std::unique_ptr<Item> Parser::parse_item() {
  auto vis_item = parse_vis_item();
  if(!vis_item) return nullptr;
  return std::make_unique<Item>(std::move(vis_item));
}


std::unique_ptr<VisItem> Parser::parse_vis_item() {
  Backtracker tracker(*_ast_ctx);
  if(auto f = parse_function(); f) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(f));
  }
  if(auto s = parse_struct(); s) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(s));
  }
  if(auto e = parse_enumeration(); e) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(e));
  }
  if(auto c = parse_constant_item(); c) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(c));
  }
  if(auto t = parse_trait(); t) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(t));
  }
  if(auto i = parse_implementation(); i) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(i));
  }
  return nullptr;
}



}