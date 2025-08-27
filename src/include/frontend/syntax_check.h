#ifndef INSOMNIA_SYNTAX_CHECK_H
#define INSOMNIA_SYNTAX_CHECK_H

#include <unordered_map>

#include "ast_recursive_visitor.h"
#include "ast_type.h"
#include "parser.h"

namespace insomnia::rust_shard::ast {

// check basic syntax (like break/continue) and collect symbols
class SymbolCollector : public RecursiveVisitor {
public:
  SymbolCollector(ErrorRecorder *recorder): _recorder(recorder) {}
  void preVisit(Crate &node) override {
    node.set_scope(std::make_unique<Scope>());
    _scopes.push_back(node.scope().get());
  }
  void postVisit(Crate &node) override {
    _scopes.pop_back();
  }
  void preVisit(BlockExpression &node) override {
    node.set_scope(std::make_unique<Scope>());
    _scopes.push_back(node.scope().get());
  }
  void postVisit(BlockExpression &node) override {
    _scopes.pop_back();
  }
  void preVisit(MatchArm &node) override {
    node.set_scope(std::make_unique<Scope>());
    _scopes.push_back(node.scope().get());
  }
  void postVisit(MatchArm &node) override {
    _scopes.pop_back();
  }
  void visit(Function &node) override {
    RecursiveVisitor::preVisit(node); // no need
    // function name register
    add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction
    });
    // function body block: into the scope first
    if(node.expr_opt()) {
      node.expr_opt()->set_scope(std::make_unique<Scope>());
      _scopes.push_back(node.expr_opt()->scope().get());
      if(node.params_opt()) node.params_opt()->accept(*this);
    }
    if(node.res_type_opt()) node.res_type_opt()->accept(*this);

    RecursiveVisitor::postVisit(node); // no need
  }

private:
  ErrorRecorder *_recorder;
  std::vector<Scope*> _scopes; // store the constructing scopes
  void add_symbol(std::string_view ident, const SymbolInfo &symbol) {
    _scopes.back()->add_symbol(ident, symbol);
  }
};

}


#endif // INSOMNIA_SYNTAX_CHECK_H













