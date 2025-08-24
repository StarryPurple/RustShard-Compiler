#include <fstream>
#include <sstream>
#include <iostream>

#include "ast.h"
#include "parser.h"
#include "lexer.h"

using Lexer = insomnia::rust_shard::Lexer;
using Parser = insomnia::rust_shard::ast::Parser;

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
    Lexer lexer(source_code);
    Parser parser(lexer);
    if(!parser) {
      std::cout << parser.error_msg() << std::endl;
      std::cout << '\n' << "Parse failed." << std::endl;
    } else {
      std::cout << "Parse all fine." << std::endl;
    }
  } catch(std::runtime_error &e) {
    std::cout << e.what() << std::endl;
  }
  return 0;
}