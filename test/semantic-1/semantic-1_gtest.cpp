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
    using namespace rs;

    Lexer lexer(source_code);
    if(!lexer) {
      std::cout << "Fail" << std::endl;
      std::cerr << lexer.error_msg() << std::endl;
      return "Fail";
    }

    ast::Parser parser(lexer);
    if(!parser) {
      std::cout << "Fail" << std::endl;
      std::cerr << "Parser error." << std::endl;
      return "Fail";
    }

    auto error_recorder = std::make_unique<ast::ErrorRecorder>();
    auto type_pool = std::make_unique<stype::TypePool>();
    auto const_pool = std::make_unique<sconst::ConstPool>();

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
      return "Fail";
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
      return "Fail";
    }
    ast::PreTypeFiller pre_type_filler(error_recorder.get(), type_pool.get());
    ast_tree.traverse(pre_type_filler);
    if(error_recorder->has_error()) {
      std::cout << "Fail" << std::endl;
      std::cerr << "Pre type declaration error." << std::endl;
      for(const auto &error: error_recorder->untagged_errors())
        std::cerr << error << std::endl;
      for(const auto &[tag, error]: error_recorder->tagged_errors())
        std::cerr << tag << ": " << error << std::endl;
      return "Fail";
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
      return "Fail";
    }
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
    std::sort(file_paths.begin(), file_paths.end());
    return file_paths;
}

INSTANTIATE_TEST_SUITE_P(
    Semantics,
    SemanticTest,
    ::testing::ValuesIn(GetAllTestFiles(TEST_DIRECTORY_PATH)), // NOLINT: defined in CMakeLists
    [](const testing::TestParamInfo<fs::path>& info) {
        return info.param.filename().replace_extension("").string();
    }
);