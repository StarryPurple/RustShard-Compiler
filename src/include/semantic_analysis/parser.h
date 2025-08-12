#ifndef INSOMNIA_PARSER_H
#define INSOMNIA_PARSER_H
#include "ast.h"
#include "lexer.h"

namespace insomnia {


class Parser {
  class ASTContext; // Token flow
  class ASTBacktracker;
  std::unique_ptr<ASTContext> _ast_ctx;
  bool _is_good;

  std::unique_ptr<ASTCrate> _crate;

  // returns an empty pointer as an error signal.

  std::unique_ptr<ASTCrate> parse_crate();
  std::unique_ptr<ASTItem> parse_item();
  std::unique_ptr<ASTVisItem> parse_vis_item();
  std::unique_ptr<ASTFunction> parse_function();
  std::unique_ptr<ASTStruct> parse_struct();
  std::unique_ptr<ASTEnumeration> parse_enumeration();
  std::unique_ptr<ASTConstantItem> parse_constant_item();
  std::unique_ptr<ASTTrait> parse_trait();
  std::unique_ptr<ASTImplementation> parse_implementation();

public:
  Parser() = default;
  explicit Parser(Lexer &lexer) { parse(lexer); }

  void parse(Lexer &lexer);

  [[nodiscard]] explicit operator bool() const { return _is_good; }
  [[nodiscard]] bool is_good() const { return _is_good; }
};

}

#endif // INSOMNIA_PARSER_H