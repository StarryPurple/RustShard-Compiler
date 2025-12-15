#include "ast_defs.h"

#include "ast.h"
#include "ast_recursive_visitor.h"

namespace insomnia::rust_shard::ast {

ASTTree::ASTTree(std::unique_ptr<Crate> crate)
: _crate(std::move(crate)) { _crate->set_name("Crate"); }

void ASTTree::traverse(RecursiveVisitor &r_visitor) {
  r_visitor.traverse(*_crate);
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

bool Scope::set_type(std::string_view ident, stype::TypePtr type) {
  auto it = _symbol_set.find(ident);
  if(it == _symbol_set.end()) return false;
  it->second.type = std::move(type);
  return true;
}

void Scope::load_builtin(stype::TypePool *pool) {
  // primitive types
  for(const auto prime: stype::type_primes()) {
    auto ident = stype::get_type_view_from_prime(prime);
    _symbol_set.emplace(ident, SymbolInfo{
      .node = nullptr,
      .ident = ident,
      .kind = SymbolKind::kPrimitiveType,
      .type = pool->make_type<stype::PrimitiveType>(prime)
    });
  }
  // primitive functions

  // fn print(s: &str) -> ()

  // fn println(s: &str) -> ()

  // fn printInt(n: i32) -> ()

  // fn printlnInt(n: i32) -> ()

  // fn getString() -> String

  // fn getInt() -> i32

  // fn exit(code: i32) -> ()

  // fn to_string(&self) -> String

  // fn as_str(&self) -> &str
  // Available on: String

  // fn as_mut_str(&mut self) -> &mut str
  // Available on: String

  // fn len(&self) -> usize
  // Available on: [T; N], &[T; N], &mut [T; N], String, &str, &mut str

  // fn from(&str) -> String
  // fn from(&mut str) -> String

  // fn append(&mut self, s: &str) -> ()
  // Available on: String
}


}
