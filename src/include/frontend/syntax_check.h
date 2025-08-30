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

  // scope related:
  // Crate, BlockExpression, MatchArms, Function, Impl, Trait

  void preVisit(Crate &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(Crate &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(BlockExpression &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(BlockExpression &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void visit(MatchArms &node) override {
    // no preVisit
    for(const auto &[arm, expr]: node.arms()) {
      _scopes.push_back(std::make_unique<Scope>());
      arm->accept(*this);
      expr->accept(*this);
      arm->set_scope(std::move(_scopes.back()));
      _scopes.pop_back();
    }
    // no postVisit
  }
  void preVisit(Function &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction
    }); // add to outer scope
    if(!flag)
      _recorder->report("Function symbol already defined: " + std::string(node.ident()));
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(Function &node) override {
    if(node.body_opt())
      node.body_opt()->set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(InherentImpl &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(InherentImpl &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(TraitImpl &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(TraitImpl &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(Trait &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kTrait
    });
    if(!flag)
      _recorder->report("Trait symbol already defined: " + std::string(node.ident()));
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(Trait &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }

  // symbol related:
  // Function(dealt), const, struct, trait(dealt), enum
  // patterns (including MatchArm pattern and Let pattern)
  void preVisit(ConstantItem &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kConstant
    });
    if(!flag)
      _recorder->report("ConstantItem symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(StructStruct &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kStruct
    });
    if(!flag)
      _recorder->report("StructStruct symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(Enumeration &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kEnum
    });
    if(!flag)
      _recorder->report("Enumeration symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(IdentifierPattern &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kVariable
    });
    if(!flag)
      _recorder->report("Variable symbol already defined: " + std::string(node.ident()));
  }

  // type related:
  // struct(dealt), enum(dealt), type alias
  void preVisit(TypeAlias &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kTypeAlias
    });
    if(!flag)
      _recorder->report("TypaAlias typename already used: " + std::string(node.ident()));
  }

private:
  ErrorRecorder *_recorder;
  std::vector<std::unique_ptr<Scope>> _scopes; // store the constructing scopes
  bool add_symbol(std::string_view ident, const SymbolInfo &symbol) {
    return _scopes.back()->add_symbol(ident, symbol);
  }
};

}


#endif // INSOMNIA_SYNTAX_CHECK_H













