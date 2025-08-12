#include "semantic_analysis/parser.h"

namespace insomnia {

class Parser::ASTContext {
  friend Parser::ASTBacktracker;
  std::vector<Token> _tokens;
  std::size_t _pos = 0;
public:
  ASTContext() = default;
  template <class T>
  explicit ASTContext(T &&tokens, std::size_t pos = 0)
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

class Parser::ASTBacktracker {
  ASTContext &_ast_ctx;
  std::size_t _pos;
  bool _commited;
public:
  ASTBacktracker(ASTContext &ast_ctx)
  : _ast_ctx(ast_ctx), _pos(ast_ctx._pos), _commited(false) {}
  ~ASTBacktracker() {
    if(!_commited) _ast_ctx._pos = _pos;
  }
  void commit() { _commited = true; }
};

void Parser::parse(Lexer &lexer) {
  _ast_ctx = std::make_unique<ASTContext>(lexer.release());
  _crate = parse_crate();
  _is_good = static_cast<bool>(_crate);
}

std::unique_ptr<ASTCrate> Parser::parse_crate() {
  std::vector<std::unique_ptr<ASTItem>> vec;
  while(!_ast_ctx->empty()) {
    auto item = parse_item();
    if(!item) return nullptr;
    vec.push_back(std::move(item));
  }
  return std::make_unique<ASTCrate>(std::move(vec));
}

std::unique_ptr<ASTItem> Parser::parse_item() {
  auto vis_item = parse_vis_item();
  if(!vis_item) return nullptr;
  return std::make_unique<ASTItem>(std::move(vis_item));
}


std::unique_ptr<ASTVisItem> Parser::parse_vis_item() {
  ASTBacktracker tracker(*_ast_ctx);
  if(auto f = parse_function(); f) {
    tracker.commit();
    return std::make_unique<ASTVisItem>(std::move(f));
  }
  if(auto s = parse_struct(); s) {
    tracker.commit();
    return std::make_unique<ASTVisItem>(std::move(s));
  }
  if(auto e = parse_enumeration(); e) {
    tracker.commit();
    return std::make_unique<ASTVisItem>(std::move(e));
  }
  if(auto c = parse_constant_item(); c) {
    tracker.commit();
    return std::make_unique<ASTVisItem>(std::move(c));
  }
  if(auto t = parse_trait(); t) {
    tracker.commit();
    return std::make_unique<ASTVisItem>(std::move(t));
  }
  if(auto i = parse_implementation(); i) {
    tracker.commit();
    return std::make_unique<ASTVisItem>(std::move(i));
  }
  return nullptr;
}



}