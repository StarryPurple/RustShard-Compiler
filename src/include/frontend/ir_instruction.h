#ifndef RUST_SHARD_IR_INSTRUCTION_H
#define RUST_SHARD_IR_INSTRUCTION_H

namespace insomnia::rust_shard::ir {

class IRType {
public:
  IRType() = default;
  explicit IRType(stype::TypePtr type): _type(std::move(type)) {}
  stype::TypePtr type() const { return _type; }
  IRType get_ref(stype::TypePool *pool) {
    return IRType(pool->make_type<stype::RefType>(_type, true));
  }
  bool is_void(stype::TypePool *pool) const {
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

  static Operand make_reg(int id, IRType type) {
    return Operand{ .kind = OperandKind::kVirtualReg, .value = id, .type = type };
  }
  static Operand make_imm(std::int64_t val, IRType type) {
    return Operand{ .kind = OperandKind::kImmediate, .value = val, .type = type };
  }

  std::string value_str() const {
    switch (kind) {
    case OperandKind::kImmediate: {
      return std::to_string(value);
    } break;
    case OperandKind::kVirtualReg: {
      return "%" + std::to_string(value);
    } break;
    }
    throw std::runtime_error("Invalid OperandKind");
  }
  std::string to_str() const {
    return type.to_str() + " " + value_str();
  }
};

struct Instruction {
  Instruction() = default;
  virtual ~Instruction() = default;
  virtual std::string to_str() const = 0;

  // std::string reg/label()...
};

// %x = alloca Ty
// LetExpression
struct AllocaInst: Instruction {
  int dst;
  IRType type;

  std::string to_str() const override {
    return "%" + std::to_string(dst) + " = alloca " + type.to_str();
  }
};

// store Ty val / %1, Ty* %x
// Assignment, CompoundAssignment
struct StoreInst: Instruction {
  Operand value, ptr;

  std::string to_str() const {
    return "store " + value.to_str() + ", " + ptr.to_str();
  }
};

// %dst = load Ty, Ty* %ptr
// PathExpr, IndexExpr?
struct LoadInst: Instruction {
  int dst;
  IRType load_type;
  Operand ptr;
  std::string to_str() const override {
    return "%" + std::to_string(dst) + " = load " + load_type.to_str() + ", "
    + ptr.to_str();
  }
};

// %3 = op Ty %1, %2
// Arithmetic, Comparison, LazyBoolean
//
// op shall be in:
// add, sub, mul, sdiv, udiv, srem, urem, shl, ashr, lshr, and, or, xor
// (icmp) eq, ne, ugt, uge, ult, ule, sgt, sge, slt, sle
struct BinaryOpInst: Instruction {
  int dst;
  StringT op;
  IRType type;
  Operand lhs, rhs;

  std::string to_str() const override {
    return "%" + std::to_string(dst) + " = " + op + " " + type.to_str()
    + " " + lhs.value_str() + ", " + rhs.value_str();
  }
};

// (%0 = ) call ret_t @func(Ty %1, ...)
// CallExpr, MethodCall
// set dst_name = "" if ret_type is void
struct CallInst: Instruction {
  int dst = -1; // meaningless if ret_type is void
  IRType ret_type;
  StringT func_name;
  std::vector<Operand> args; // type and reg name

  std::string to_str() const override {
    std::string res;
    if(ret_type.to_str() != "void") {
      if(dst == -1) {
        throw std::runtime_error("non-void function result not recorded");
      }
      res += "%" + std::to_string(dst) + " = ";
    }
    res += "call " + ret_type.to_str() + " @" + func_name + "(";
    for(int i = 0; i < args.size(); ++i) {
      if(i > 0) res += ", ";
      res += args[i].to_str();
    }
    res += ")";
    return res;
  }
};

// ret Ty %0 / ret void
// ReturnExpression, FuncBody (return at end)
struct ReturnInst: Instruction {
  std::optional<Operand> ret_val;

  std::string to_str() const override {
    return "ret " + (ret_val.has_value() ? ret_val->to_str() : "void");
  }
};

// (T*) %dst = getelementptr [N x T], [N x T]* %ptr, i32 0, index_t idx
// (T*) %dst = getelementptr S, S* %ptr, i32 0, index_t idx
// Array, Index, Field
struct GEPInst: Instruction {
  int dst;
  IRType base_type;
  Operand ptr;
  std::vector<Operand> indices;

  std::string to_str() const override {
    std::string res = "%" + std::to_string(dst) + " = getelementptr " + base_type.to_str() + ", "
      + ptr.to_str();
    for(auto &operand: indices)
      res += ", " + operand.to_str();
    return res;
  }
};

// like static cast, casts value to value
// %c = bitcast(or something else) Ty1 %p to Ty2
// TypeCast
struct CastInst: Instruction {
  int dst;
  IRType dst_type;
  Operand src;

  static int bit_width(stype::TypePrime prime) {
    switch(prime) {
    case stype::TypePrime::kBool: return 1;
    case stype::TypePrime::kI8: case stype::TypePrime::kU8: case stype::TypePrime::kChar: return 8;
    case stype::TypePrime::kI16: case stype::TypePrime::kU16: return 16;
    case stype::TypePrime::kI32: case stype::TypePrime::kU32: return 32;
    case stype::TypePrime::kI64: case stype::TypePrime::kU64: return 64;
    case stype::TypePrime::kISize: case stype::TypePrime::kUSize: return 64;
    default: throw std::runtime_error("Unsupported prime type for bit width");
    }
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
      ? "zext" : "sext";
    }
    return "trunc";
  }
  std::string to_str() const override {
    return "%" + std::to_string(dst) + " = " + cast_op() + " "
    + src.to_str() + " to " + dst_type.to_str();
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
  int label_id;
  LabelHint hint;
  int hint_tag_id;

  Label() = default;
  Label(LabelHint _hint, int _hint_tag_id):
  label_id(-1), hint(_hint), hint_tag_id(_hint_tag_id) {}

  std::string to_str() const {
    if(label_id == -1) {
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
    return "_" + std::to_string(label_id) + "_" + map.at(hint) + "." + std::to_string(hint_tag_id);
  }
};

// br label %next
// if, loop, block (end)
struct BranchInst: Instruction {
  Label label;

  std::string to_str() const override {
    return "br label %" + label.to_str();
  }
};

// br i1 %cond, label %then, label %else
// if, while (condition)
struct CondBranchInst: Instruction {
  int cond;
  Label true_label, false_label;

  std::string to_str() const override {
    return "br i1 %" + std::to_string(cond) + ", label %" + true_label.to_str()
    + ", label %" + false_label.to_str();
  }
};

// unreachable
// exit, break, continue
struct UnreachableInst: Instruction {
  std::string to_str() const override {
    return "unreachable";
  }
};

// %0 = insertvalue %Struct/Array undef, Ty val/%reg, 0
// ...
// %n = insertvalue %Struct/Array %n-1, Ty val/%reg, n
// Attention: All these n instructions are all packed in one InsertValueInst.
struct InsertValueInst: Instruction {
  IRType type; // %Struct/Array
  std::vector<int> interval_regs; // "0", ... "n"
  std::vector<Operand> operands;

  std::string to_str() const override {
    std::string res;
    for(int i = 0; i < operands.size(); ++i) {
      if(i > 0) res += "\n  ";
      res += "%" + std::to_string(interval_regs[i]) + " = insertvalue " + type.to_str() + " ";
      res += (i > 0 ? ("%" + interval_regs[i - 1]) : "undef");
      res += ", " + operands[i].to_str();
      res += ", " + std::to_string(i);
    }
    return res;
  }
};

// %dst = phi Ty [val1/%res1, %label1], [val2/%res2, %label2], ..., [valn/%resn, %labeln]
struct PhiInst: Instruction {
  int dst;
  IRType type;
  std::vector<std::pair<Operand, Label>> incoming;
  std::string to_str() const override {
    std::string res = "%" + std::to_string(dst) + " = phi " + type.to_str() + " ";
    for(std::size_t i = 0; i < incoming.size(); ++i) {
      if(i > 0) res += ", ";
      res += "[" + incoming[i].first.value_str() + ", %" + incoming[i].second.to_str() + "]";
    }
    return res;
  }
};

}

#endif // RUST_SHARD_IR_INSTRUCTION_H