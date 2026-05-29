#ifndef RUST_SHARD_IR_INSTRUCTION_H
#define RUST_SHARD_IR_INSTRUCTION_H

#include <unordered_map>
#include <optional>
#include <variant>

#include "common/stype.hpp"

namespace rshard::ir {

class IrType {
public:
  IrType() = default;
  explicit IrType(stype::TypePtr type): _type(std::move(type)) {}
  stype::TypePtr type() const { return _type; }
  std::size_t size() const { return _type->size(); }
  std::size_t align() const { return _type->align(); }

  IrType get_ref(stype::TypePool* pool) {
    return IrType(pool->make_type<stype::RefType>(_type, true));
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

struct Operand {
  struct VRegister { reg_id_t reg; bool operator==(const VRegister& o) const { return reg == o.reg; } };
  struct Immediate { imm_val_t imm; bool operator==(const Immediate& o) const { return imm == o.imm; } };
  std::variant<VRegister, Immediate> value;
  IrType type;

  struct Hash {
    std::size_t operator()(const Operand& oper) const {
      std::size_t seed = std::hash<int>{}(static_cast<int>(oper.value.index())) * 0x52a7c0b9;
      return std::visit([&seed](const auto& v) -> std::size_t {
        using T = std::decay_t<decltype(v)>;
        std::size_t h = 0;
        if constexpr(std::is_same_v<T, VRegister>) {
          h = std::hash<reg_id_t>{}(static_cast<int>(v.reg));
        } else if constexpr(std::is_same_v<T, Immediate>) {
          h = std::hash<imm_val_t>{}(v.imm);
        }
        return seed ^ (h + 0x9e3779b9 + (seed << 6) + (seed >> 2));
      }, oper.value);
    }
  };

  static Operand make_reg(reg_id_t reg, IrType type) {
    return Operand{.value = {VRegister{reg}}, .type = type};
  }

  static Operand make_imm(imm_val_t imm, IrType type) {
    return Operand{.value = {Immediate{imm}}, .type = type};
  }

  bool is_imm() const { return std::holds_alternative<Immediate>(value); }
  bool is_reg() const { return std::holds_alternative<VRegister>(value); }

  reg_id_t as_reg() const { return std::get<VRegister>(value).reg; }
  imm_val_t as_imm() const { return std::get<Immediate>(value).imm; }

  void set_reg(reg_id_t reg) {
    if(is_reg()) value = Immediate{reg};
  }

  void replace_reg(reg_id_t old_reg, Operand new_op) {
    if(is_reg() && as_reg() == old_reg) {
      value = new_op.value;
    }
  }

  void rename_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) {
    if(is_reg() && reorder_map.contains(as_reg())) {
      value = VRegister{reorder_map.at(as_reg())};
    }
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
    if(operand.is_reg()) {
      return hinted_reg(operand.as_reg());
    }
    if(operand.is_imm()) {
      return std::to_string(operand.as_imm());
    }
    throw std::runtime_error("Invalid OperandKind");
  }

  std::string hinted_operand(const Operand& operand) const {
    return operand.type.to_str() + " " + hinted_operand_data(operand);
  }
};

using instr_no_t = std::uint32_t;

struct Instruction {
  instr_no_t instr_no{};

  Instruction() = default;
  virtual ~Instruction() = default;

  // the reg defined by this instruction
  virtual std::optional<reg_id_t> get_dst() const { return std::nullopt; }

  // modify the reg defined by this instruction
  virtual void set_dst(reg_id_t) {}

  // return all regs used by this instruction
  virtual std::vector<reg_id_t> get_uses() const { return {}; }

  // replace all reg usages
  virtual void replace_use(reg_id_t old_reg, Operand new_op) {}

  virtual void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) {}
};

// %x = alloca Ty
// LetExpression
struct AllocaInst: Instruction {
  reg_id_t dst;
  IrType type;

  std::optional<reg_id_t> get_dst() const override { return dst; }
  void set_dst(reg_id_t id) override { dst = id; }
};

// store Ty val / %1, Ty* %x
// Assignment, CompoundAssignment
struct StoreInst: Instruction {
  Operand value, ptr;

  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(value.is_reg()) uses.push_back(value.as_reg());
    if(ptr.is_reg()) uses.push_back(ptr.as_reg());
    return uses;
  }

  void replace_use(reg_id_t old_reg, Operand new_op) override {
    value.replace_reg(old_reg, new_op);
    ptr.replace_reg(old_reg, new_op);
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    value.rename_reg(reorder_map);
    ptr.rename_reg(reorder_map);
  }
};

// %dst = load Ty, Ty* %ptr
// PathExpr, IndexExpr?
struct LoadInst: Instruction {
  reg_id_t dst;
  IrType load_type;
  Operand ptr;

  std::optional<reg_id_t> get_dst() const override { return dst; }
  void set_dst(reg_id_t id) override { dst = id; }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(ptr.is_reg()) uses.push_back(ptr.as_reg());
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    ptr.replace_reg(old_reg, new_op);
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    ptr.rename_reg(reorder_map);
  }
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
  IrType type;
  Operand lhs, rhs;

  std::optional<reg_id_t> get_dst() const override { return dst; }
  void set_dst(reg_id_t id) override { dst = id; }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(lhs.is_reg()) uses.push_back(lhs.as_reg());
    if(rhs.is_reg()) uses.push_back(rhs.as_reg());
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    lhs.replace_reg(old_reg, new_op);
    rhs.replace_reg(old_reg, new_op);
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    lhs.rename_reg(reorder_map);
    rhs.rename_reg(reorder_map);
  }
};

// (%0 = ) call ret_t @func(Ty %1, ...)
// CallExpr, MethodCall
// set dst_name = "" if ret_type is void
struct CallInst: Instruction {
  std::optional<reg_id_t> dst; // meaningless if ret_type is void
  IrType ret_type;
  StringT func_name;
  std::vector<Operand> args; // type and reg name

  std::optional<reg_id_t> get_dst() const override {
    return dst;
  }
  void set_dst(reg_id_t id) override { dst = id; }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    for(auto& arg: args) {
      if(arg.is_reg()) uses.push_back(arg.as_reg());
    }
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    for(auto& arg: args) {
      arg.replace_reg(old_reg, new_op);
    }
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    for(auto& arg: args) {
      arg.rename_reg(reorder_map);
    }
  }
};

// ret Ty %0 / ret void
// ReturnExpression, FuncBody (return at end)
struct ReturnInst: Instruction {
  std::optional<Operand> ret_val;

  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(ret_val && ret_val->is_reg())
      uses.push_back(ret_val->as_reg());
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    if(ret_val)
      ret_val->replace_reg(old_reg, new_op);
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    if(ret_val)
      ret_val->rename_reg(reorder_map);
  }
};

// (T*) %dst = getelementptr [N x T], [N x T]* %ptr, i32 0, index_t idx
// (T*) %dst = getelementptr S, S* %ptr, i32 0, index_t idx
// Array, Index, Field
struct GEPInst: Instruction {
  reg_id_t dst;
  IrType base_type;
  Operand ptr;
  std::vector<Operand> indices;

  std::optional<reg_id_t> get_dst() const override { return dst; }
  void set_dst(reg_id_t id) override { dst = id; }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(ptr.is_reg()) uses.push_back(ptr.as_reg());
    for(auto& idx: indices) {
      if(idx.is_reg()) uses.push_back(idx.as_reg());
    }
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    ptr.replace_reg(old_reg, new_op);
    for(auto& idx: indices) {
      idx.replace_reg(old_reg, new_op);
    }
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    ptr.rename_reg(reorder_map);
    for(auto& idx: indices) {
      idx.rename_reg(reorder_map);
    }
  }
};

// like static cast, casts value to value
// %c = bitcast(or something else) Ty1 %p to Ty2
// TypeCast
struct CastInst: Instruction {
  reg_id_t dst;
  IrType dst_type;
  Operand src;

  std::optional<reg_id_t> get_dst() const override { return dst; }
  void set_dst(reg_id_t id) override { dst = id; }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(src.is_reg()) uses.push_back(src.as_reg());
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    src.replace_reg(old_reg, new_op);
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    src.rename_reg(reorder_map);
  }

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
  kCriticalEdge,
};

struct Label {
  block_id_t block_id;
  LabelHint hint;
  hint_id_t hint_id;

  Label() = default;

  Label(LabelHint _hint, hint_id_t _hint_id):
    block_id(-1), hint(_hint), hint_id(_hint_id) {}

  auto operator<=>(const Label&) const = default;

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
        {LabelHint::kLazyExit, "lazy.exit"}, {LabelHint::kCriticalEdge, "critical_edge"},
      };
    return "_" + std::to_string(block_id) + "." + map.at(hint) + "." + std::to_string(hint_id);
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
  Operand cond;
  Label true_label, false_label;

  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    if(cond.is_reg()) uses.push_back(cond.as_reg());
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    cond.replace_reg(old_reg, new_op);
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    cond.rename_reg(reorder_map);
  }
};

// unreachable
// exit, break, continue
struct UnreachableInst: Instruction {};

// %0 = insertvalue %Struct/Array undef, Ty val/%reg, 0
// ...
// %n = insertvalue %Struct/Array %n-1, Ty val/%reg, n
// Attention: All these n instructions are all packed in one InsertValueInst.
struct InsertValueInst: Instruction {
  IrType type;                    // %Struct/Array
  std::vector<int> interval_regs; // "0", ... "n"
  std::vector<Operand> operands;

  std::optional<reg_id_t> get_dst() const override {
    return interval_regs.back();
  }
  void set_dst(reg_id_t id) override {
    interval_regs.back() = id;
  }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    for(auto& op: operands) {
      if(op.is_reg()) uses.push_back(op.as_reg());
    }
    for(std::size_t i = 0; i + 1 < interval_regs.size(); ++i) {
      uses.push_back(interval_regs[i]);
    }
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    for(auto& op: operands) {
      op.replace_reg(old_reg, new_op);
    }
    for(std::size_t i = 0; i + 1 < interval_regs.size(); ++i) {
      if(interval_regs[i] == old_reg) {
        // assert(new_op.is_reg());
        interval_regs[i] = new_op.as_reg();
      }
    }
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    for(auto& op: operands) {
      op.rename_reg(reorder_map);
    }
    for(std::size_t i = 0; i + 1 < interval_regs.size(); ++i) {
      if(reorder_map.contains(interval_regs[i])) {
        interval_regs[i] = reorder_map.at(interval_regs[i]);
      }
    }
  }
};

// %dst = phi Ty [val1/%res1, %label1], [val2/%res2, %label2], ..., [valn/%resn, %labeln]
struct PhiInst: Instruction {
  struct Income {
    Operand oper;
    Label label;
  };
  reg_id_t original_slot = -2; // for PromoteAlloca. The original value shall not matter.
  reg_id_t dst;
  IrType type;
  std::vector<Income> incoming;

  std::optional<reg_id_t> get_dst() const override { return dst; }
  void set_dst(reg_id_t id) override { dst = id; }
  std::vector<reg_id_t> get_uses() const override {
    std::vector<reg_id_t> uses;
    for(auto& [op, _label]: incoming) {
      if(op.is_reg()) uses.push_back(op.as_reg());
    }
    return uses;
  }
  void replace_use(reg_id_t old_reg, Operand new_op) override {
    for(auto& [op, _label]: incoming) {
      op.replace_reg(old_reg, new_op);
    }
  }
  void rename_use_reg(const std::unordered_map<reg_id_t, reg_id_t>& reorder_map) override {
    for(auto& [op, _label]: incoming) {
      op.rename_reg(reorder_map);
    }
  }
};
}

#endif // RUST_SHARD_IR_INSTRUCTION_H
