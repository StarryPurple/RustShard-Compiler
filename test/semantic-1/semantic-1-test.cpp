#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <stdexcept>

// Include your core compiler headers
#include "ast.h"
#include "parser.h"
#include "lexer.h"

namespace fs = std::filesystem;
using Lexer  = insomnia::rust_shard::Lexer;
using Parser = insomnia::rust_shard::ast::Parser;

struct TestResult {
  std::string name;
  std::string status;
  std::string expected;
  std::string actual;
};

// 重用之前的函数，无需修改
std::string read_file(const fs::path &path) {
  std::ifstream file(path);
  if(!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

std::string get_expected_verdict(const std::string &source_code) {
  std::regex verdict_regex(R"(Verdict:\s*(\w+))");
  std::smatch matches;
  if(std::regex_search(source_code, matches, verdict_regex) && matches.size() > 1) {
    return matches[1].str();
  }
  return "Unknown";
}

std::string run_compiler_logic(const std::string &source_code) {
  try {
    Lexer lexer(source_code);
    if(!lexer) return "Fail";

    Parser parser(lexer);
    if(!parser) return "Fail";

    return "Success";
  } catch(const std::runtime_error &e) {
    return "Fail";
  }
}

int main(int argc, char *argv[]) {
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path_to_test_directory>" << std::endl;
    return 1;
  }

  fs::path test_dir = argv[1];

  std::cout << "Starting semantic-1 tests..." << std::endl;
  std::cout << "Test directory: " << test_dir << std::endl;

  if(!fs::exists(test_dir)) {
    std::cerr << "Error: Test directory not found at " << test_dir << std::endl;
    return 1;
  }

  std::vector<TestResult> results;
  int pass_count = 0;
  int fail_count = 0;

  for(const auto &entry : fs::recursive_directory_iterator(test_dir)) {
    if(entry.is_regular_file() && entry.path().extension() == ".rx") {
      TestResult result;
      result.name = entry.path().parent_path().filename().string();

      try {
        std::cout << "Running test: " << result.name << " ... " << std::flush;
        std::string source_code = read_file(entry.path());

        result.expected = get_expected_verdict(source_code);
        result.actual   = run_compiler_logic(source_code);

        if(result.actual == result.expected) {
          result.status = "PASS";
          pass_count++;
        } else {
          result.status = "FAIL";
          fail_count++;
        }
        std::cout << "[" << result.status << "]" << std::endl;
      } catch(const std::exception &e) {
        result.status = "FAIL";
        result.actual = "Exception";
        fail_count++;
        std::cout << "[FAIL] (Exception: " << e.what() << ")" << std::endl;
      }
      results.push_back(result);
    }
  }

  std::cout << "\n" << std::string(40, '=') << std::endl;
  std::cout << "Test Summary" << std::endl;
  std::cout << std::string(40, '=') << std::endl;
  for(const auto &r : results) if(r.status == "FAIL") {
    std::cout << "[" << r.status << "] " << r.name << " (Expected: " << r.expected << ", Actual: " << r.actual << ")" <<
      std::endl;
  }

  std::cout << "\nTotal: " << results.size() << " | Passed: " << pass_count << " | Failed: " << fail_count << std::endl;

  return (fail_count > 0) ? 1 : 0;
}
