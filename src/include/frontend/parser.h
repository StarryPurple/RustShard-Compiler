#ifndef INSOMNIA_PARSER_H
#define INSOMNIA_PARSER_H
#include "ast.h"
#include "lexer.h"

#define PARSE_FUNCTION_GEN_METHOD(Node) \
  std::unique_ptr<Node> parse##Node();

namespace insomnia::rust_shard::ast {

class Parser {
  class Context; // Token flow
  class Backtracker;
  bool _is_good = false;
  std::unique_ptr<Context> _ast_ctx;
  std::unique_ptr<Crate> _crate;

  // returns an empty pointer as failure signal.

  INSOMNIA_RUST_SHARD_AST_NODES_LIST(PARSE_FUNCTION_GEN_METHOD)

public:
  Parser() = default;
  explicit Parser(Lexer &lexer) { parseAll(lexer); }

  void parseAll(Lexer &lexer);

  explicit operator bool() const { return _is_good; }
  bool is_good() const { return _is_good; }
  std::string error_msg() const;
};

}

#undef PARSE_FUNCTION_GEN_METHOD
#endif // INSOMNIA_PARSER_H