#include "ast_defs.h"

#include "ast.h"
#include "ast_recursive_visitor.h"

namespace insomnia::rust_shard::ast {

ASTTree::ASTTree(std::unique_ptr<Crate> crate)
: _crate(std::move(crate)) { _crate->set_name(_crate_name); }

void ASTTree::traverse(RecursiveVisitor &r_visitor) {
  r_visitor.traverse(*_crate);
}

void Scope::load_builtin_types(sem_type::TypePool *pool) {
  for(const auto prime: sem_type::type_primes()) {
    auto ident = sem_type::get_type_view_from_prime(prime);
    _symbol_set.emplace(ident, SymbolInfo{
      .node = nullptr,
      .ident = ident,
      .kind = SymbolKind::kPrimitiveType,
      .type = pool->make_type<sem_type::PrimitiveType>(prime)
    });
  }
}

SymbolInfo* Scope::add_symbol(std::string_view ident, const SymbolInfo &symbol) {
  if(_symbol_set.contains(ident)) return nullptr;
  _symbol_set.emplace(ident, symbol);
  return find_symbol(ident);
}

SymbolInfo* Scope::find_symbol(std::string_view ident) {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return nullptr;
  return &it->second;
}

const SymbolInfo* Scope::find_symbol(std::string_view ident) const {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return nullptr;
  return &it->second;
}

bool Scope::set_type(std::string_view ident, sem_type::TypePtr type) {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return false;
  it->second.type = std::move(type);
  return true;
}

}
