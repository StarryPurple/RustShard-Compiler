#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <stdexcept>
#include <sstream>

#include "ast.h"
#include "parser.h"
#include "lexer.h"
#include "syntax_check.h"

namespace fs = std::filesystem;
namespace rs = insomnia::rust_shard;
using Lexer = rs::Lexer;
using Parser = rs::ast::Parser;
using ErrorRecorder = rs::ast::ErrorRecorder;
using SymbolCollector = rs::ast::SymbolCollector;

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

    auto err_recorder = std::make_unique<ErrorRecorder>();
    SymbolCollector symbol_collector(err_recorder.get());

    auto ast = parser.release_tree();
    ast.traverse(symbol_collector);
    if(err_recorder->has_error()) return "Fail";

    return "Success";
  } catch(const std::runtime_error &e) {
    return "Fail";
  }
}

class SemanticTest : public ::testing::TestWithParam<fs::path> {};

TEST_P(SemanticTest, SemanticCheck) {
  fs::path test_file_path = GetParam();

  std::string test_name = test_file_path.parent_path().filename().string() + "/" + test_file_path.filename().string();

  try {
    std::string source_code = read_file(test_file_path);
    std::string expected_verdict = get_expected_verdict(source_code);
    std::string actual_verdict = run_compiler_logic(source_code);

    ASSERT_EQ(actual_verdict, expected_verdict)
      << "Test file: " << test_name << " failed.\n"
      << "Expected: " << expected_verdict << "\n"
      << "Actual: " << actual_verdict;

  } catch (const std::exception& e) {
    FAIL() << "Test file: " << test_name << " threw an exception: " << e.what();
  }
}

std::vector<fs::path> GetAllTestFiles(const std::string& test_dir) {
    std::vector<fs::path> file_paths;
    if (!fs::exists(test_dir)) {
        std::cerr << "Warning: Test directory not found at " << test_dir << std::endl;
        return file_paths;
    }
    for (const auto& entry : fs::recursive_directory_iterator(test_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".rx") {
            file_paths.push_back(entry.path());
        }
    }
    return file_paths;
}

INSTANTIATE_TEST_SUITE_P(
    AllSemanticTests,
    SemanticTest,
    ::testing::ValuesIn(GetAllTestFiles(TEST_DIRECTORY_PATH)), // NOLINT: defined in CMakeLists
    [](const testing::TestParamInfo<fs::path>& info) {
        std::string filename = info.param.filename().string();
        std::string parent_dir = info.param.parent_path().filename().string();
        return parent_dir + "_" + filename.substr(0, filename.find('.'));
    }
);