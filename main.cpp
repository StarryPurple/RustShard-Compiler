#include "semantic_analysis/lexer.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::string read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if(!file) throw std::runtime_error("Failed to open file.");
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

int main() {
  std::string path = "../testcases/semantic-1/basic28/basic28.rx";
  std::string source_code = read_file(path);
  source_code = " /* we can /* nest /* deeply */ nested */ \n comments */ \n fn main() { exit(0); }";
  insomnia::Lexer lexer(source_code);
  if(!lexer) {
    std::cout << "Compile error" << std::endl;
    return 0;
  }
  for(auto &item: lexer.tokens()) {
    std::cout << item << std::endl;
  }
  return 0;
}