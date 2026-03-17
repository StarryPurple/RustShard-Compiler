#include <fstream>
#include <iostream>

#include "frontend/ast.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/syntax_check.hpp"
#include "frontend/ir_generator.hpp"
#include "common/printer.hpp"

namespace fs = std::filesystem;

#ifndef BUILTIN_LL_PATH
#define BUILTIN_LL_PATH "builtin/builtin.ll"
#endif

std::string read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if(!file) throw std::runtime_error("Failed to open input file.");
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

int main() {
  std::string src_code;
  std::string line;
  while(std::getline(std::cin, line)) {
    src_code += line + "\n";
  }

  auto error_recorder = std::make_unique<rshard::ast::ErrorRecorder>();
  auto type_pool = std::make_unique<rshard::stype::TypePool>();
  auto const_pool = std::make_unique<rshard::sconst::ConstPool>();
  rshard::ast::ASTTree ast_tree;

  // semantic
  try {
    rshard::Lexer lexer(src_code);
    if(!lexer) {
      return 1;
    }

    rshard::ast::Parser parser(lexer);
    if(!parser) {
      return 1;
    }

    ast_tree = parser.release_tree();

    rshard::ast::SymbolCollector symbol_collector(error_recorder.get());
    ast_tree.traverse(symbol_collector);
    if(error_recorder->has_error()) {
      return 1;
    }

    rshard::ast::TypeDeclarator type_declarator(error_recorder.get(), type_pool.get());
    ast_tree.traverse(type_declarator);
    if(error_recorder->has_error()) {
      return 1;
    }

    rshard::ast::PreTypeFiller pre_type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
    ast_tree.traverse(pre_type_filler);
    if(error_recorder->has_error()) {
      return 1;
    }

    rshard::ast::TypeFiller type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
    ast_tree.traverse(type_filler);
    if(error_recorder->has_error()) {
      return 1;
    }
  } catch(...) {
    // failure in semantic check
    return 1;
  }

  // ir generation
  try {
    rshard::ir::IRGenerator ir_generator(type_pool.get());
    ast_tree.traverse(ir_generator);

    auto ir_pack = ir_generator.release();

    std::cout << read_file(BUILTIN_LL_PATH);
    std::cout << rshard::ir::IrPrinter::sprint(ir_pack);
  } catch(...) {
    // ir generation error
    // return 0 (since semantic check passed)
    return 0;
  }

  return 0;
}