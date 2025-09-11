#include <fstream>
#include <sstream>
#include <iostream>

#include "ast.h"
#include "parser.h"
#include "lexer.h"
#include "syntax_check.h"

namespace rs = insomnia::rust_shard;

std::string read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if(!file) throw std::runtime_error("Failed to open file.");
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

int main() {
  // std::string path = "../test/semantic-1/return1/return1.rx";
  std::string path = "../test/sem.rx";
  std::string source_code = read_file(path);
  try {
    do {
      using namespace rs;

      Lexer lexer(source_code);
      if(!lexer) {
        std::cout << "Fail" << std::endl;
        std::cerr << lexer.error_msg() << std::endl;
        break;
      }

      ast::Parser parser(lexer);
      if(!parser) {
        std::cout << "Fail" << std::endl;
        std::cerr << "Parser error." << std::endl;
        break;
      }

      auto error_recorder = std::make_unique<ast::ErrorRecorder>();
      auto type_pool = std::make_unique<sem_type::TypePool>();
      auto const_pool = std::make_unique<sem_const::ConstPool>();

      auto ast_tree = parser.release_tree();

      ast::SymbolCollector symbol_collector(error_recorder.get());
      ast_tree.traverse(symbol_collector);
      if(error_recorder->has_error()) {
        std::cout << "Fail" << std::endl;
        std::cerr << "Symbol collection error." << std::endl;
        for(const auto &error: error_recorder->untagged_errors())
          std::cerr << error << std::endl;
        for(const auto &[tag, error]: error_recorder->tagged_errors())
          std::cerr << tag << ": " << error << std::endl;
        break;
      }

      ast::TypeDeclarator type_declarator(error_recorder.get(), type_pool.get());
      ast_tree.traverse(type_declarator);
      if(error_recorder->has_error()) {
        std::cout << "Fail" << std::endl;
        std::cerr << "Type declaration error." << std::endl;
        for(const auto &error: error_recorder->untagged_errors())
          std::cerr << error << std::endl;
        for(const auto &[tag, error]: error_recorder->tagged_errors())
          std::cerr << tag << ": " << error << std::endl;
        break;
      }

      ast::TypeFiller type_filler(error_recorder.get(), type_pool.get(), const_pool.get());
      ast_tree.traverse(type_filler);
      if(error_recorder->has_error()) {
        std::cout << "Fail" << std::endl;
        std::cerr << "Type filling error." << std::endl;
        for(const auto &error: error_recorder->untagged_errors())
          std::cerr << error << std::endl;
        for(const auto &[tag, error]: error_recorder->tagged_errors())
          std::cerr << tag << ": " << error << std::endl;
        break;
      }

      std::cout << "Success" << std::endl;
    } while(false);
  } catch(std::runtime_error &e) {
    std::cout << "runtime error: " << e.what() << std::endl;
  }
  return 0;
}