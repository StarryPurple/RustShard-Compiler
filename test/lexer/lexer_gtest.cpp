#include "gtest/gtest.h"
#include "frontend/lexer.h"

namespace ism = insomnia;
using TKT = ism::TokenType;

std::vector<std::pair<TKT, std::string>> get_token_pairs(const std::string &source_code) {
  ism::Lexer lexer(source_code);
  EXPECT_TRUE(lexer.is_good()) << lexer.error_msg();
  std::vector<std::pair<TKT, std::string>> res;
  for(auto &token: lexer.tokens()) {
    res.emplace_back(token.token_type, std::string(token.lexeme));
  }
  return res;
}

// expected_tokens should be parenthesized like a initializer list.
// Refer to the samples.
#define TEST_LEXER_SUCCESS(test_name, source_code, expected_tokens) \
  TEST(LexerTest, test_name) { \
    auto tokens = get_token_pairs(source_code); \
    auto expected = std::vector<std::pair<ism::TokenType, std::string>> expected_tokens; \
    ASSERT_EQ(tokens, expected); \
  }

#define TEST_LEXER_FAILURE(test_name, source_code) \
  TEST(LexerTest, test_name) { \
    ism::Lexer lexer(source_code); \
    ASSERT_FALSE(lexer.is_good()); \
  }

// --- Keywords and Identifiers ---
TEST_LEXER_SUCCESS(KeywordsAndIdentifiers,
  "let mut my_var: i32 = true;",
  ({
    {TKT::LET, "let"}, {TKT::MUT, "mut"}, {TKT::IDENTIFIER, "my_var"},
    {TKT::COLON, ":"}, {TKT::IDENTIFIER, "i32"}, {TKT::EQ, "="},
    {TKT::TRUE, "true"}, {TKT::SEMI, ";"}
  }));

TEST_LEXER_SUCCESS(AllKeywords,
  "as break const continue crate else enum extern false fn for if impl in "
  "let loop match mod move mut pub ref return self Self super trait true type "
  "unsafe use where while async await dyn",
  ({
    {TKT::AS, "as"}, {TKT::BREAK, "break"}, {TKT::CONST, "const"},
    {TKT::CONTINUE, "continue"}, {TKT::CRATE, "crate"}, {TKT::ELSE, "else"},
    {TKT::ENUM, "enum"}, {TKT::EXTERN, "extern"}, {TKT::FALSE, "false"},
    {TKT::FN, "fn"}, {TKT::FOR, "for"}, {TKT::IF, "if"},
    {TKT::IMPL, "impl"}, {TKT::IN, "in"}, {TKT::LET, "let"},
    {TKT::LOOP, "loop"}, {TKT::MATCH, "match"}, {TKT::MOD, "mod"},
    {TKT::MOVE, "move"}, {TKT::MUT, "mut"}, {TKT::PUB, "pub"},
    {TKT::REF, "ref"}, {TKT::RETURN, "return"}, {TKT::SELF_OBJECT, "self"},
    {TKT::SELF_TYPE, "Self"}, {TKT::SUPER, "super"}, {TKT::TRAIT, "trait"},
    {TKT::TRUE, "true"}, {TKT::TYPE, "type"}, {TKT::UNSAFE, "unsafe"},
    {TKT::USE, "use"}, {TKT::WHERE, "where"}, {TKT::WHILE, "while"},
    {TKT::ASYNC, "async"}, {TKT::AWAIT, "await"}, {TKT::DYN, "dyn"}
  }));

// --- Numeric Literals ---
TEST_LEXER_SUCCESS(DecimalNumbers,
  "let n = 123_456; let f = 1.2345;",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "n"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "123_456"}, {TKT::SEMI, ";"},
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "f"}, {TKT::EQ, "="},
    {TKT::FLOAT_LITERAL, "1.2345"}, {TKT::SEMI, ";"}
  }));

TEST_LEXER_SUCCESS(IntegerSuffix,
  "let a = 123u32; let b = 456_isize;",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "a"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "123u32"}, {TKT::SEMI, ";"},
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "b"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "456_isize"}, {TKT::SEMI, ";"}
  }));

// --- String Literals ---
TEST_LEXER_SUCCESS(StringWithEscapes,
  "let s = \"hello\\nworld\";",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "s"}, {TKT::EQ, "="},
    {TKT::STRING_LITERAL, "\"hello\\nworld\""}, {TKT::SEMI, ";"}
  }));

// --- Operators and Delimiters ---
TEST_LEXER_SUCCESS(ArithmeticOperators,
  "a + b - c * d / e % f",
  ({
    {TKT::IDENTIFIER, "a"}, {TKT::PLUS, "+"}, {TKT::IDENTIFIER, "b"},
    {TKT::MINUS, "-"}, {TKT::IDENTIFIER, "c"}, {TKT::STAR, "*"},
    {TKT::IDENTIFIER, "d"}, {TKT::SLASH, "/"}, {TKT::IDENTIFIER, "e"},
    {TKT::PERCENT, "%"}, {TKT::IDENTIFIER, "f"}
  }));

TEST_LEXER_SUCCESS(ComparisonOperators,
  "a == b != c < d > e <= f >= g",
  ({
    {TKT::IDENTIFIER, "a"}, {TKT::EQ_EQ, "=="}, {TKT::IDENTIFIER, "b"},
    {TKT::NE, "!="}, {TKT::IDENTIFIER, "c"}, {TKT::LT, "<"},
    {TKT::IDENTIFIER, "d"}, {TKT::GT, ">"}, {TKT::IDENTIFIER, "e"},
    {TKT::LE, "<="}, {TKT::IDENTIFIER, "f"}, {TKT::GE, ">="},
    {TKT::IDENTIFIER, "g"}
  }));

TEST_LEXER_SUCCESS(CompoundOperators,
  "a += b; c *= d; e /= f; g %= h; i ^= j;",
  ({
    {TKT::IDENTIFIER, "a"}, {TKT::PLUS_EQ, "+="}, {TKT::IDENTIFIER, "b"}, {TKT::SEMI, ";"},
    {TKT::IDENTIFIER, "c"}, {TKT::STAR_EQ, "*="}, {TKT::IDENTIFIER, "d"}, {TKT::SEMI, ";"},
    {TKT::IDENTIFIER, "e"}, {TKT::SLASH_EQ, "/="}, {TKT::IDENTIFIER, "f"}, {TKT::SEMI, ";"},
    {TKT::IDENTIFIER, "g"}, {TKT::PERCENT_EQ, "%="}, {TKT::IDENTIFIER, "h"}, {TKT::SEMI, ";"},
    {TKT::IDENTIFIER, "i"}, {TKT::CARET_EQ, "^="}, {TKT::IDENTIFIER, "j"}, {TKT::SEMI, ";"}
  }));

TEST_LEXER_SUCCESS(MiscOperatorsAndDelimiters,
  "let a: Vec<i32> = vec![1, 2, 3];",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "a"}, {TKT::COLON, ":"},
    {TKT::IDENTIFIER, "Vec"}, {TKT::LT, "<"}, {TKT::IDENTIFIER, "i32"},
    {TKT::GT, ">"}, {TKT::EQ, "="}, {TKT::IDENTIFIER, "vec"},
    {TKT::NOT, "!"}, {TKT::L_SQUARE_BRACKET, "["}, {TKT::INTEGER_LITERAL, "1"},
    {TKT::COMMA, ","}, {TKT::INTEGER_LITERAL, "2"}, {TKT::COMMA, ","},
    {TKT::INTEGER_LITERAL, "3"}, {TKT::R_SQUARE_BRACKET, "]"}, {TKT::SEMI, ";"}
  }));

// --- Comments and Whitespace ---
TEST_LEXER_SUCCESS(SingleLineComment,
  "let x = 1; // This is a comment\nlet y = 2;",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "x"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "1"}, {TKT::SEMI, ";"},
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "y"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "2"}, {TKT::SEMI, ";"}
  }));

TEST_LEXER_SUCCESS(MultiLineComment,
  "let a = 1; /* This is a nested /*\nmulti-/*li*/ne\nco*/mment. */ let b = 2;",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "a"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "1"}, {TKT::SEMI, ";"},
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "b"}, {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "2"}, {TKT::SEMI, ";"}
  }));

// "1.2.foo()" is valid, so I decide not to check what's after the second dot in lexer.
TEST_LEXER_SUCCESS(ConfusingNumberLiteral,
  "let s = 1.2.3;",
  ({
    {TKT::LET, "let"}, {TKT::IDENTIFIER, "s"}, {TKT::EQ, "="},
    {TKT::FLOAT_LITERAL, "1.2"}, {TKT::DOT, "."}, {TKT::INTEGER_LITERAL, "3"},
    {TKT::SEMI, ";"}
  }));

TEST_LEXER_SUCCESS(
  NumberWithDot,
  "let num = 123.foo;",
  ({
    {TKT::LET, "let"},
    {TKT::IDENTIFIER, "num"},
    {TKT::EQ, "="},
    {TKT::INTEGER_LITERAL, "123"},
    {TKT::DOT, "."},
    {TKT::IDENTIFIER, "foo"},
    {TKT::SEMI, ";"}
  }));

// --- Error Cases ---
TEST_LEXER_FAILURE(UnterminatedString,
  "let s = \"hello;");

TEST_LEXER_FAILURE(UnclosedMultiLineComment,
  "let x = 1; /* unclosed comment");