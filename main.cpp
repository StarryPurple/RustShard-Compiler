#include <fstream>
#include <iostream>

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "syntax_check.h"
#include "IR_generator.h"

namespace rs = insomnia::rust_shard;

int main() {
  std::string src_code;
  std::string line;
  while(std::getline(std::cin, line)) {
    src_code += line + "\n";
  }

  rs::Lexer lexer(src_code);
  if(!lexer) {
    return 1;
  }

  rs::ast::Parser parser(lexer);
  if(!parser) {
    return 1;
  }

  auto error_recorder = std::make_unique<rs::ast::ErrorRecorder>();
  auto type_pool = std::make_unique<rs::stype::TypePool>();
  auto const_pool = std::make_unique<rs::sconst::ConstPool>();

  auto ast_tree = parser.release_tree();

  rs::ast::SymbolCollector symbol_collector(error_recorder.get());
  ast_tree.traverse(symbol_collector);
  if(error_recorder->has_error()) {
    return 1;
  }

  rs::ast::TypeDeclarator type_declarator(error_recorder.get(), type_pool.get());
  ast_tree.traverse(type_declarator);
  if(error_recorder->has_error()) {
    return 1;
  }

  rs::ast::PreTypeFiller pre_type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
  ast_tree.traverse(pre_type_filler);
  if(error_recorder->has_error()) {
    return 1;
  }

  rs::ast::TypeFiller type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
  ast_tree.traverse(type_filler);
  if(error_recorder->has_error()) {
    return 1;
  }

  rs::ir::IRGenerator ir_generator(type_pool.get());
  ast_tree.traverse(ir_generator);

  std::cout << ir_generator.IR_str();

  return 0;
}