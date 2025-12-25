#include "defs.h"

#include "ast.h"
#include "recursive_visitor.h"

namespace insomnia::rust_shard::ast {

ASTTree::ASTTree(std::unique_ptr<Crate> crate)
: _crate(std::move(crate)) { _crate->set_name("Crate"); }

void ASTTree::traverse(RecursiveVisitor &r_visitor) {
  r_visitor.traverse(*_crate);
}

SymbolInfo* Scope::add_symbol(const StringRef &ident, const SymbolInfo &symbol) {
  if(_symbol_set.contains(ident)) {
    throw std::runtime_error("Symbol \"" + std::string(ident) + "\" already added");
    return nullptr;
  }
  _symbol_set.emplace(ident, symbol);
  return find_symbol(ident);
}

SymbolInfo* Scope::find_symbol(const StringRef &ident) {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return nullptr;
  return &it->second;
}

const SymbolInfo* Scope::find_symbol(const StringRef &ident) const {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return nullptr;
  return &it->second;
}

bool Scope::set_type(const StringRef &ident, stype::TypePtr type) {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return false;
  it->second.type = std::move(type);
  return true;
}

}
