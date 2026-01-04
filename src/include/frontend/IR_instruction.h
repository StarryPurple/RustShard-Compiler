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

struct Instruction {
  Instruction() = default;
  virtual ~Instruction() = default;
  virtual std::string to_str() const = 0;

  // std::string reg/label()...
};

// %x = alloca Ty
// LetExpression
struct AllocaInst: Instruction {
  StringT dst_name;
  IRType type;

  std::string to_str() const override {
    return "%" + dst_name + " = alloca " + type.to_str();
  }
};

// store Ty val / %1, ptr %x
// Assignment, CompoundAssignment
struct StoreInst: Instruction {
  bool is_instant = false;
  StringT value_or_name, ptr_name;
  IRType value_type, ptr_type;

  std::string to_str() const {
    return "store " + value_type.to_str() + " " + (is_instant ? "" : "%") + value_or_name + ", "
    + ptr_type.to_str() + " %" + ptr_name;
  }
};

// %dst = load Ty, Ty* %ptr
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
// (icmp) eq, ne, ugt, uge, ult, ule, sgt, sge, slt, sle
struct BinaryOpInst: Instruction {
  bool is_l_instant, is_r_instant;
  StringT dst, lhs, rhs, op;
  IRType type;
  std::string to_str() const override {
    return "%" + dst + " = " + op + " " + type.to_str()
    + (is_l_instant ? " " : " %") + lhs + ", " + (is_r_instant ? "" : "%") + rhs;
  }
};

// (%0 = ) call ret_t @func(Ty %1, ...)
// CallExpr, MethodCall
// set dst_name = "" if ret_type is void
struct CallInst: Instruction {
  StringT dst_name; // "" if ret_type is void
  IRType ret_type;
  StringT func_name;
  std::vector<std::pair<IRType, StringT>> args; // type and reg name

  std::string to_str() const override {
    std::string res;
    if(ret_type.to_str() != "void")
      res += "%" + dst_name + " = ";
    res += "call " + ret_type.to_str() + " @" + func_name + "(";
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
// set ret_val = "" for return void
struct ReturnInst: Instruction {
  IRType ret_type;
  StringT ret_val; // "" for return void

  std::string to_str() const override {
    if(ret_type.to_str() == "void") return "ret void";
    return "ret " + ret_type.to_str() + " %" + ret_val;
  }
};

// (T*) %dst = getelementptr [N x T], [N x T]* %ptr, i32 0, index_t idx
// (T*) %dst = getelementptr S, S* %ptr, i32 0, index_t idx
// Array, Index, Field
struct GEPInst: Instruction {
  StringT dst_name, ptr_name;
  IRType base_type, ptr_type;
  IRType index_type;
  bool is_idx_instant;
  std::string index_name_or_value;

  std::string to_str() const override {
    return "%" + dst_name + " = getelementptr " + base_type.to_str() + ", "
      + ptr_type.to_str() + " %" + ptr_name + ", i32 0, " + index_type.to_str() + " "
      + (is_idx_instant ? "" : "%") + index_name_or_value;
  }
};

// %c = bitcast(or something else) Ty1* %p to Ty2*
// TypeCast
struct CastInst: Instruction {
  StringT src_name, dst_name;
  IRType src_type, dst_type;
  bool must_be_bitcast = false, is_static_src = false;

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
    if(must_be_bitcast) return "bitcast";
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
    + src_type.to_str() + (is_static_src ? " @" : " %") + src_name
    + " to " + dst_type.to_str();
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

// %0 = insertvalue %Struct/Array undef, Ty val/%reg, 0
// ...
// %n = insertvalue %Struct/Array %n-1, Ty val/%reg, n
// Attention: All these n instructions are all packed in one InsertValueInst.
struct InsertValueInst: Instruction {
  struct Info {
    IRType field_ty;
    bool is_instant;
    std::string value_or_name;
  };

  IRType type; // %Struct/Array
  std::vector<std::string> interval_regs; // "0", ... "n"
  std::vector<Info> infos;

  std::string to_str() const override {
    std::string res;
    for(int i = 0; i < infos.size(); ++i) {
      if(i > 0) res += "\n  ";
      res += "%" + interval_regs[i] + " = insertvalue " + type.to_str() + " ";
      res += (i > 0 ? ("%" + interval_regs[i - 1]) : "undef");
      res += ", " + infos[i].field_ty.to_str() + " " + (infos[i].is_instant ? "" : "%") + infos[i].value_or_name;
      res += ", " + std::to_string(i);
    }
    return res;
  }
};

// %dst = phi Ty [%res1, %label1], [%res2, %label2]
struct PhiInst: Instruction {
  bool is_res1_instant, is_res2_instant;
  std::string dst_name, res1_name_or_value, res2_name_or_value;
  IRType dst_type;
  std::string label1, label2;

  std::string to_str() const override {
    return "%" + dst_name + " = phi " + dst_type.to_str() + " [" + (is_res1_instant ? "" : "%")
    + res1_name_or_value + ", %" + label1 + "], ["
    + (is_res2_instant ? "" : "%") + res2_name_or_value + ", %" + label2 + "]";
  }
};

}

#endif // RUST_SHARD_IR_INSTRUCTION_H