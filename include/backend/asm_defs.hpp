#ifndef RUST_SHARD_ASM_OPERAND_HPP
#define RUST_SHARD_ASM_OPERAND_HPP

#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace rshard::backend {

enum class PhysReg : uint8_t {
  zero = 0, ra = 1, sp = 2, gp = 3,
  tp = 4, t0 = 5, t1 = 6, t2 = 7,
  s0 = 8, s1 = 9,
  a0 = 10, a1 = 11, a2 = 12, a3 = 13,
  a4 = 14, a5 = 15, a6 = 16, a7 = 17,
  s2 = 18, s3 = 19, s4 = 20, s5 = 21,
  s6 = 22, s7 = 23, s8 = 24, s9 = 25,
  s10 = 26, s11 = 27,
  t3 = 28, t4 = 29, t5 = 30, t6 = 31,
};

const std::unordered_map<PhysReg, std::string> kRegNames = {
  {PhysReg::zero, "zero"}, {PhysReg::ra, "ra"},   {PhysReg::sp, "sp"},
  {PhysReg::gp, "gp"},     {PhysReg::tp, "tp"},
  {PhysReg::t0, "t0"},     {PhysReg::t1, "t1"},   {PhysReg::t2, "t2"},
  {PhysReg::s0, "s0"},     {PhysReg::s1, "s1"},
  {PhysReg::a0, "a0"},     {PhysReg::a1, "a1"},   {PhysReg::a2, "a2"},
  {PhysReg::a3, "a3"},     {PhysReg::a4, "a4"},   {PhysReg::a5, "a5"},
  {PhysReg::a6, "a6"},     {PhysReg::a7, "a7"},
  {PhysReg::s2, "s2"},     {PhysReg::s3, "s3"},   {PhysReg::s4, "s4"},
  {PhysReg::s5, "s5"},     {PhysReg::s6, "s6"},   {PhysReg::s7, "s7"},
  {PhysReg::s8, "s8"},     {PhysReg::s9, "s9"},   {PhysReg::s10, "s10"},
  {PhysReg::s11, "s11"},
  {PhysReg::t3, "t3"},     {PhysReg::t4, "t4"},   {PhysReg::t5, "t5"},
  {PhysReg::t6, "t6"},
};

constexpr PhysReg kTmpRd  = PhysReg::t0;
constexpr PhysReg kTmpRs1 = PhysReg::t1;
constexpr PhysReg kTmpRs2 = PhysReg::t2;

const std::unordered_set kAllocatableRegs = {
  // PhysReg::t0, PhysReg::t1, PhysReg::t2, // never in use
  PhysReg::s0, PhysReg::s1,
  PhysReg::a0, PhysReg::a1, PhysReg::a2, PhysReg::a3,
  PhysReg::a4, PhysReg::a5, PhysReg::a6, PhysReg::a7,
  PhysReg::s2, PhysReg::s3, PhysReg::s4, PhysReg::s5,
  PhysReg::s6, PhysReg::s7, PhysReg::s8, PhysReg::s9,
  PhysReg::s10, PhysReg::s11,
  PhysReg::t3, PhysReg::t4, PhysReg::t5, PhysReg::t6,
};

const std::unordered_set kCallerSaveRegs = {
  // PhysReg::t0, PhysReg::t1, PhysReg::t2, // never in use
  PhysReg::a0, PhysReg::a1, PhysReg::a2, PhysReg::a3,
  PhysReg::a4, PhysReg::a5, PhysReg::a6, PhysReg::a7,
  PhysReg::t3, PhysReg::t4, PhysReg::t5, PhysReg::t6,
};

const std::unordered_set kCalleeSaveRegs = {
  PhysReg::s0, PhysReg::s1, PhysReg::s2, PhysReg::s3,
  PhysReg::s4, PhysReg::s5, PhysReg::s6, PhysReg::s7,
  PhysReg::s8, PhysReg::s9, PhysReg::s10, PhysReg::s11,
};

} // namespace rshard::backend

#endif