#ifndef RUST_SHARD_IR_INSTRUCTION_H
#define RUST_SHARD_IR_INSTRUCTION_H

namespace insomnia::rust_shard::ir {

class IRType {
public:
  IRType() = default;
  explicit IRType(stype::TypePtr type): _type(std::move(type)) {}
  stype::TypePtr type() const { return _type; }
  void add_ptr(stype::TypePool *pool) {
    _type = pool->make_type<stype::RefType>(_type, true);
  }
  std::string to_str() const {
    if(auto t = _type.get_if<stype::TupleType>(); t && t->members().empty()) {
      return "void";
    }
    return _type->IR_string();
  }
private:
  stype::TypePtr _type;
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
  StringT reg;
  IRType type;

  std::string to_str() const override {
    return "%" + reg + " = alloca " + type.to_str();
  }
};

// store Ty val / %1, ptr %x
// Assignment, CompoundAssignment
struct StoreInst: Instruction {
  bool is_instant = false;
  StringT value_name, ptr_name;
  IRType value_type, ptr_type;

  std::string to_str() const {
    return "store " + value_type.to_str() + " " + (is_instant ? "" : "%") + value_name + ", "
    + ptr_type.to_str() + " %" + ptr_name;
  }
};

// %1 = load Ty, ptr %x
// PathExpr, IndexExpr?
struct LoadInst: Instruction {
  StringT dst_name, ptr_name;
  IRType load_type, ptr_type;
  std::string to_str() const override {
    return "%" + dst_name + " = load " + load_type.to_str() + ", "
    + ptr_type.to_str() + " %" + ptr_name;
  }
};

// %3 = op Ty %1, %2
// Arithmetic, Comparison, LazyBoolean
//
// op shall be in:
// add, sub, mul, sdiv, udiv, srem, urem, shl, ashr, lshr, and, or, xor
// (is_cmp=true) eq, ne, ugt, uge, ult, ule, sgt, sge, slt, sle
struct BinaryOpInst: Instruction {
  bool is_cmp;
  StringT dst, lhs, rhs, op;
  IRType type;
  std::string to_str() const override {
    return "%" + dst + " = " + (is_cmp ? "icmp " : "") + op + " " + type.to_str() + " %" + lhs + ", %" + rhs;
  }
};

// call ret_t @func(Ty %1, ...)
// CallExpr, MethodCall
struct CallInst: Instruction {
  StringT dst_name; // "" if ret_type is void
  IRType ret_type;
  StringT func_name;
  std::vector<std::pair<IRType, StringT>> args;

  std::string to_str() const override {
    std::string res;
    if(ret_type.to_str() != "void")
      res += "%" + dst_name + " = ";
    res += "call " + ret_type.to_str() + " " + func_name + "(";
    for(int i = 0; i < args.size(); ++i) {
      if(i > 0) res += ", ";
      res += args[i].first.to_str() + " %" + args[i].second;
    }
    res += ")";
    return res;
  }
};

// ret Ty %0 / ret void
// ReturnExpression, FuncBody (return at end)
struct ReturnInst: Instruction {
  IRType ret_type;
  StringT ret_val; // "" for return void

  std::string to_str() const override {
    if(ret_type.to_str() == "void") return "ret void";
    return "ret " + ret_type.to_str() + " %" + ret_val;
  }
};

// %p = getelementptr Ty, Ty* %ptr, [i32 idx]+
// Array, Index, Field
struct GEPInst: Instruction {
  StringT dst_name, ptr_name;
  IRType base_type, ptr_type;
  std::vector<std::tuple<IRType, bool, std::string>> indices; // type + is_instant + ident

  std::string to_str() const override {
    std::string res = "%" + dst_name + " = getelementptr " + base_type.to_str() + ptr_type.to_str() + " %" + ptr_name;
    for(auto &[tp, is_instant, val_name]: indices) {
      res += ", " + tp.to_str() + (is_instant ? "" : "%") + val_name;
    }
    return res;
  }
};

// %c = bitcast Ty1* %p to Ty2*
// TypeCast
struct CastInst: Instruction {
  StringT src_name, dst_name;
  IRType src_type, dst_type;

  static int bit_width(stype::TypePrime prime) {
    switch(prime) {
    case stype::TypePrime::kI8: case stype::TypePrime::kU8: return 8;
    case stype::TypePrime::kI16: case stype::TypePrime::kU16: return 16;
    case stype::TypePrime::kI32: case stype::TypePrime::kU32: return 32;
    case stype::TypePrime::kI64: case stype::TypePrime::kU64: return 64;
    case stype::TypePrime::kISize: case stype::TypePrime::kUSize: return 64;
    default: throw std::runtime_error("Unsupported prime type for bit width");
    }
  }
  std::string cast_op() const {
    auto src = src_type.type().get_if<stype::PrimeType>(), dst = dst_type.type().get_if<stype::PrimeType>();
    if(!src || !dst) {
      throw std::runtime_error("Cast between types that is not prime");
    }
    auto src_width = bit_width(src->prime()), dst_width = bit_width(dst->prime());
    if(src_width == dst_width) return "bitcast";
    if(src_width < dst_width)
      return src->is_unsigned_int() ? "zext" : "sext";
    return "trunc";
  }
  std::string to_str() const override {
    return "%" + dst_name + " = " + cast_op() + " "
    + src_type.to_str() + " %" + src_name + " to " + dst_type.to_str();
  }
};

// br label %next
// if, loop, block (end)
struct BranchInst: Instruction {
  StringT label;

  std::string to_str() const override {
    return "br label %" + label;
  }
};

// br i1 %cond, label %then, label %else
// if, while (condition)
struct CondBranchInst: Instruction {
  StringT cond_name, true_label, false_label;

  std::string to_str() const override {
    return "br i1 %" + cond_name + ", label %" + true_label + ", label %" + false_label;
  }
};

// unreachable
// exit, break, continue
struct UnreachableInst: Instruction {
  std::string to_str() const override {
    return "unreachable";
  }
};

}

#endif // RUST_SHARD_IR_INSTRUCTION_H