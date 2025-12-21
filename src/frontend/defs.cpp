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
    throw std::runtime_error("Symbol" + std::string(ident) + " already added");
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

void Scope::load_builtin(stype::TypePool *pool) {
  using namespace stype;

  // primitive types
  for(const auto prime: type_primes()) {
    auto ident = prime_strs(prime);
    _symbol_set.emplace(ident, SymbolInfo{
      .node = nullptr,
      .ident = ident,
      .kind = SymbolKind::kPrimitiveType,
      .type = pool->make_type<PrimeType>(prime)
    });
  }

  // builtin functions
  static const std::vector<std::pair<StringRef, TypePtr>> builtin_functions = {{
      // fn print(s: &str) -> ()
      "print", pool->make_type<FunctionType>(
      "print",
      std::vector{
        pool->make_type<RefType>(pool->make_type<PrimeType>(TypePrime::kString), false),
      },
      pool->make_unit())
    }, {
      // fn println(s: &str) -> ()
      "println", pool->make_type<FunctionType>(
      "println",
      std::vector{
        pool->make_type<RefType>(pool->make_type<PrimeType>(TypePrime::kString), false),
      },
      pool->make_unit()
      )
    }, {
      // fn printInt(n: i32) -> ()
      "printInt", pool->make_type<FunctionType>(
      "printInt",
      std::vector{
        pool->make_type<PrimeType>(TypePrime::kI32),
      },
      pool->make_unit()
      )
    }, {
      // fn printlnInt(n: i32) -> ()
      "printlnInt", pool->make_type<FunctionType>(
      "printlnInt",
      std::vector{
        pool->make_type<PrimeType>(TypePrime::kI32),
      },
      pool->make_unit()
      )
    }, {
      // fn getString() -> String
      "getString", pool->make_type<FunctionType>(
        "getString",
        std::vector<TypePtr>{},
        pool->make_type<PrimeType>(TypePrime::kString)
      )
    }, {
      // fn getInt() -> i32
      "getInt", pool->make_type<FunctionType>(
        "getInt",
        std::vector<TypePtr>{},
        pool->make_type<PrimeType>(TypePrime::kI32)
      )
    }, {
      // fn exit(code: i32) -> ()
      "exit", pool->make_type<FunctionType>(
      "exit",
      std::vector{
        pool->make_type<PrimeType>(TypePrime::kI32),
      },
      pool->make_unit()
      )
    },/* {
      // fn to_string(&self) -> String
      // Available on primitive types...
    }, {
      // fn as_str(&self) -> &str
      // Available on: String
    }, {
      // fn as_mut_str(&mut self) -> &mut str
      // Available on: String
    }, {
      // fn len(&self) -> usize
      // Available on: [T; N], &[T; N], &mut [T; N], String, &str, &mut str
    }, {
      // fn from(&str) -> String
    }, {
      // fn from(&mut str) -> String
    }, {
      // fn append(&mut self, s: &str) -> ()
      // Available on: String
    }*/
  };
  for(const auto &[ident, type]: builtin_functions) {
    _symbol_set.emplace(ident, SymbolInfo{
      .node = nullptr,
      .ident = ident,
      .kind = SymbolKind::kFunction,
      .type = type
    });
  }
}

}
