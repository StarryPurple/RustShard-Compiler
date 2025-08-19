#ifndef INSOMNIA_AST_ENUMS_H
#define INSOMNIA_AST_ENUMS_H

namespace insomnia::rust_shard::ast {

enum class Operator {
  INVALID,

  // Arithmetic Operators
  ADD,      // TokenType::PLUS
  SUB,      // TokenType::MINUS
  MUL,      // TokenType::STAR
  DIV,      // TokenType::SLASH
  MOD,      // TokenType::PERCENT
  POW,      // TokenType::CARET  (Often represented by `^` in other languages, though Rust uses `*` for multiplication, so this might be for bitwise XOR)

  // Compound Assignment Operators
  ADD_ASSIGN, // TokenType::PLUS_EQ
  SUB_ASSIGN, // TokenType::MINUS_EQ
  MUL_ASSIGN, // TokenType::STAR_EQ
  DIV_ASSIGN, // TokenType::SLASH_EQ
  MOD_ASSIGN, // TokenType::PERCENT_EQ
  POW_ASSIGN, // TokenType::CARET_EQ

  // Logical and Comparison Operators
  AND,      // TokenType::AND
  OR,       // TokenType::OR
  NOT,      // TokenType::NOT
  LOGICAL_AND, // TokenType::AND_AND
  LOGICAL_OR,  // TokenType::OR_OR
  EQ,       // TokenType::EQ_EQ
  NE,       // TokenType::NE
  GT,       // TokenType::GT
  LT,       // TokenType::LT
  GE,       // TokenType::GE
  LE,       // TokenType::LE

  // Bitwise Operators
  BITWISE_AND, // TokenType::AND
  BITWISE_OR,  // TokenType::OR
  BITWISE_XOR, // TokenType::CARET
  SHL,         // TokenType::SHL
  SHR,         // TokenType::SHR
  BITWISE_AND_ASSIGN, // TokenType::AND_EQ
  BITWISE_OR_ASSIGN,  // TokenType::OR_EQ
  SHL_ASSIGN,         // TokenType::SHL_EQ
  SHR_ASSIGN,         // TokenType::SHR_EQ

  // Special Assignment
  ASSIGN,   // TokenType::EQ

  // Pointer/Dereference
  DEREF,    // TokenType::STAR (Can also be multiplication)
  REF,      // TokenType::AND (Can also be bitwise AND)
};

}

#endif // INSOMNIA_AST_ENUMS_H