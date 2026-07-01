#include <fstream>
#include <iostream>
#include <filesystem>

#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/syntax_check.hpp"
#include "ir/ir_generator.hpp"
#include "ir/optimizer.hpp"
#include "backend/asm_generator.hpp"
#include "backend/asm_printer.hpp"

namespace fs = std::filesystem;

#ifndef BUILTIN_ASM_PATH
#define BUILTIN_ASM_PATH "./builtin/builtin.s"
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
    // if(line == "EndOfFile") break;
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
      std::cout << "Lexer failed\n";
      return 1;
    }

    rshard::ast::Parser parser(lexer);
    if(!parser) {
      std::cout << "Parser Failed\n";
      return 1;
    }

    ast_tree = parser.release_tree();

    rshard::ast::SymbolCollector symbol_collector(error_recorder.get());
    ast_tree.traverse(symbol_collector);
    if(error_recorder->has_error()) {
      std::cout << "Symbol collector failed\n";
      return 1;
    }

    rshard::ast::TypeDeclarator type_declarator(error_recorder.get(), type_pool.get());
    ast_tree.traverse(type_declarator);
    if(error_recorder->has_error()) {
      std::cout << "Type Declarator failed\n";
      return 1;
    }

    rshard::ast::PreTypeFiller pre_type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
    ast_tree.traverse(pre_type_filler);
    if(error_recorder->has_error()) {
      std::cout << "Pre type filler failed\n";
      return 1;
    }

    rshard::ast::TypeFiller type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
    ast_tree.traverse(type_filler);
    if(error_recorder->has_error()) {
      std::cout << "Type filler failed\n";
      return 1;
    }
  } catch(...) {
    // failure in semantic check
    std::cout << "Semantic check failed due to unrecognized reason\n";
    return 1;
  }

  // asm generation
  try {
    rshard::ir::IRGenerator ir_generator(type_pool.get());
    ast_tree.traverse(ir_generator);

    auto ir_pack = ir_generator.release();

    rshard::ir::PromoteAlloca::optimize(ir_pack);
    rshard::ir::FunctionInline::optimize(ir_pack);

    rshard::backend::AsmGenerator asm_generator(ir_pack);
    auto asm_pack = asm_generator.generate();

    auto ASM = read_file(BUILTIN_ASM_PATH);
    ASM += "\n\n" + rshard::backend::AsmPrinter::sprint(asm_pack);
    std::cout << ASM;
  } catch(std::exception& e) {
    // asm generation error
    std::cerr << "[exception] " << e.what() << std::endl;
    // return 0 (since semantic check passed)
    return 0;
  } catch(...) {
    std::cerr << "[unknown exception]" << std::endl;
    return 0;
  }

  return 0;
}
