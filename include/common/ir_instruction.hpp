#ifndef RUST_SHARD_IR_INSTRUCTION_H
#define RUST_SHARD_IR_INSTRUCTION_H

#include <unordered_map>
#include "common/stype.hpp"

namespace rshard::ir {

class IRType {
public:
  IRType() = default;
  explicit IRType(stype::TypePtr type): _type(std::move(type)) {}
  stype::TypePtr type() const { return _type; }

  IRType get_ref(stype::TypePool* pool) {
    return IRType(pool->make_type<stype::RefType>(_type, true));
  }

  bool is_void(stype::TypePool* pool) const {
    return _type == pool->make_never() || _type == pool->make_unit();
  }

  std::string to_str() const {
    if(auto t = _type.get_if<stype::TupleType>(); t && t->members().empty()) {
      return "void";
    }
    return _type->IR_string();
  }

  explicit operator bool() const { return static_cast<bool>(_type); }

private:
  stype::TypePtr _type;
};

enum class OperandKind {
  kImmediate, // x
  // kStatic, // @x
  kVirtualReg, // %x
};

struct Operand {
  OperandKind kind;
  std::int64_t value;
  IRType type;

  static Operand make_reg(reg_id_t id, IRType type) {
    return Operand{.kind = OperandKind::kVirtualReg, .value = id, .type = type};
  }

  static Operand make_imm(std::int64_t val, IRType type) {
    return Operand{.kind = OperandKind::kImmediate, .value = val, .type = type};
  }
};

struct HintContext {
  const std::unordered_map<reg_id_t, std::string>* hints = nullptr;

  std::string hinted_reg(reg_id_t reg) const {
    std::string res = "%" + std::to_string(reg);
    if constexpr(kEnableVarHints) {
      if(hints) {
        auto it = hints->find(reg);
        if(it != hints->end()) res = "%" + it->second + "-" + std::to_string(reg);
      }
    }
    return res;
  }

  std::string hinted_operand_data(const Operand& operand) const {
    switch(operand.kind) {
    case OperandKind::kImmediate: {
      return std::to_string(operand.value);
    }
    break;
    case OperandKind::kVirtualReg: {
      return hinted_reg(operand.value);
    }
    break;
    }
    throw std::runtime_error("Invalid OperandKind");
  }

  std::string hinted_operand(const Operand& operand) const {
    return operand.type.to_str() + " " + hinted_operand_data(operand);
  }
};

struct Instruction {
  Instruction() = default;
  virtual ~Instruction() = default;
};

// %x = alloca Ty
// LetExpression
struct AllocaInst: Instruction {
  reg_id_t dst;
  IRType type;
};

// store Ty val / %1, Ty* %x
// Assignment, CompoundAssignment
struct StoreInst: Instruction {
  Operand value, ptr;
};

// %dst = load Ty, Ty* %ptr
// PathExpr, IndexExpr?
struct LoadInst: Instruction {
  reg_id_t dst;
  IRType load_type;
  Operand ptr;
};

// %3 = op Ty %1, %2
// Arithmetic, Comparison, LazyBoolean
//
// op shall be in:
// add, sub, mul, sdiv, udiv, srem, urem, shl, ashr, lshr, and, or, xor
// (icmp) eq, ne, ugt, uge, ult, ule, sgt, sge, slt, sle
struct BinaryOpInst: Instruction {
  reg_id_t dst;
  StringT op;
  IRType type;
  Operand lhs, rhs;
};

// (%0 = ) call ret_t @func(Ty %1, ...)
// CallExpr, MethodCall
// set dst_name = "" if ret_type is void
struct CallInst: Instruction {
  reg_id_t dst = -1; // meaningless if ret_type is void
  IRType ret_type;
  StringT func_name;
  std::vector<Operand> args; // type and reg name
};

// ret Ty %0 / ret void
// ReturnExpression, FuncBody (return at end)
struct ReturnInst: Instruction {
  std::optional<Operand> ret_val;
};

// (T*) %dst = getelementptr [N x T], [N x T]* %ptr, i32 0, index_t idx
// (T*) %dst = getelementptr S, S* %ptr, i32 0, index_t idx
// Array, Index, Field
struct GEPInst: Instruction {
  reg_id_t dst;
  IRType base_type;
  Operand ptr;
  std::vector<Operand> indices;
};

// like static cast, casts value to value
// %c = bitcast(or something else) Ty1 %p to Ty2
// TypeCast
struct CastInst: Instruction {
  reg_id_t dst;
  IRType dst_type;
  Operand src;

  std::string cast_op() const {
    auto srct = src.type.type().get_if<stype::PrimeType>(), dstt = dst_type.type().get_if<stype::PrimeType>();
    if(!srct || !dstt) {
      throw std::runtime_error("Cast between types that is not prime");
    }
    auto src_width = bit_width(srct->prime()), dst_width = bit_width(dstt->prime());
    if(src_width == dst_width) return "bitcast";
    if(src_width < dst_width) {
      return (srct->is_unsigned_int() ||
               srct->prime() == stype::TypePrime::kBool ||
               srct->prime() == stype::TypePrime::kChar)
               ? "zext"
               : "sext";
    }
    return "trunc";
  }

private:
  static int bit_width(stype::TypePrime prime) {
    switch(prime) {
    case stype::TypePrime::kBool: return 1;
    case stype::TypePrime::kI8:
    case stype::TypePrime::kU8:
    case stype::TypePrime::kChar: return 8;
    case stype::TypePrime::kI16:
    case stype::TypePrime::kU16: return 16;
    case stype::TypePrime::kI32:
    case stype::TypePrime::kU32: return 32;
    case stype::TypePrime::kI64:
    case stype::TypePrime::kU64: return 64;
    case stype::TypePrime::kISize:
    case stype::TypePrime::kUSize: return 64;
    default: throw std::runtime_error("Unsupported prime type for bit width");
    }
  }
};

enum class LabelHint {
  kEntry,
  kArrayCond,
  kArrayBody,
  kArrayExit,
  kIfThen,
  kIfElse,
  kIfExit,
  kWhileCond,
  kWhileBody,
  kWhileExit,
  kLoopBody,
  kLoopExit,
  kLazyThen,
  kLazyElse,
  kLazyExit,
};

struct Label {
  block_id_t block_id;
  LabelHint hint;
  hint_id_t hint_id;

  Label() = default;

  Label(LabelHint _hint, hint_id_t _hint_id):
    block_id(-1), hint(_hint), hint_id(_hint_id) {}

  std::string to_str() const {
    if(block_id == -1) {
      throw std::runtime_error("Label id remains invalid (-1).");
    }
    static const std::unordered_map<LabelHint, std::string> map = {
        {LabelHint::kEntry, "entry"}, {LabelHint::kArrayCond, "array.cond"},
        {LabelHint::kArrayBody, "array.body"}, {LabelHint::kArrayExit, "array.exit"},
        {LabelHint::kIfThen, "if.then"}, {LabelHint::kIfElse, "if.else"},
        {LabelHint::kIfExit, "if.exit"}, {LabelHint::kWhileCond, "while.cond"},
        {LabelHint::kWhileBody, "while.body"}, {LabelHint::kWhileExit, "while.exit"},
        {LabelHint::kLoopBody, "loop.body"}, {LabelHint::kLoopExit, "loop.exit"},
        {LabelHint::kLazyThen, "lazy.then"}, {LabelHint::kLazyElse, "lazy.else"},
        {LabelHint::kLazyExit, "lazy.exit"},
      };
    return "_" + std::to_string(block_id) + "_" + map.at(hint) + "." + std::to_string(hint_id);
  }
};

// br label %next
// if, loop, block (end)
struct BranchInst: Instruction {
  Label label;
};

// br i1 %cond, label %then, label %else
// if, while (condition)
struct CondBranchInst: Instruction {
  reg_id_t cond;
  Label true_label, false_label;
};

// unreachable
// exit, break, continue
struct UnreachableInst: Instruction {};

// %0 = insertvalue %Struct/Array undef, Ty val/%reg, 0
// ...
// %n = insertvalue %Struct/Array %n-1, Ty val/%reg, n
// Attention: All these n instructions are all packed in one InsertValueInst.
struct InsertValueInst: Instruction {
  IRType type;                    // %Struct/Array
  std::vector<int> interval_regs; // "0", ... "n"
  std::vector<Operand> operands;
};

// %dst = phi Ty [val1/%res1, %label1], [val2/%res2, %label2], ..., [valn/%resn, %labeln]
struct PhiInst: Instruction {
  reg_id_t dst;
  IRType type;
  std::vector<std::pair<Operand, Label>> incoming;
};
}

#endif // RUST_SHARD_IR_INSTRUCTION_H
