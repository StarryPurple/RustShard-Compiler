#ifndef INSOMNIA_PARSER_H
#define INSOMNIA_PARSER_H
#include "ast.h"
#include "lexer.h"

namespace insomnia::rust_shard::ast {


class Parser {
  class Context; // Token flow
  class Backtracker;
  std::unique_ptr<Context> _ast_ctx;
  bool _is_good = false;

  std::unique_ptr<Crate> _crate;

  // returns an empty pointer as an error signal.

public:
  Parser() = default;
  explicit Parser(Lexer &lexer) { parseAll(lexer); }

  void parseAll(Lexer &lexer);

  [[nodiscard]] explicit operator bool() const { return _is_good; }
  [[nodiscard]] bool is_good() const { return _is_good; }
};

}

#endif // INSOMNIA_PARSER_H