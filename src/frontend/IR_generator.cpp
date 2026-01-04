#include "IR_generator.h"

#include <algorithm>
#include <format>
#include "IR_instruction.h"

namespace insomnia::rust_shard::ir {

// %Struct = type { type-1, ... }
struct IRGenerator::TypeDeclarationPack {
  StringT ident;
  std::vector<IRType> field_types;

  std::string to_str() const {
    std::string res = "%" + ident + " = type { ";
    for(int i = 0; i < field_types.size(); ++i) {
      if(i > 0) res += ", ";
      res += field_types[i].to_str();
    }
    res += " }";
    return res;
  }
};

// constant values are all inlined (ignored here)

// For static string literals.
// @str = private unnamed_addr constant [N x i8] c"xxx\00", align 1
struct IRGenerator::StaticPack {
  StringT ident;
  StringT literal;

  static std::string interpretation_string(const StringT &literal) {
    std::string str;
    for(const auto &ch: literal) {
      switch(ch) {
      case '\n': str += "\\n"; break;
      case '\t': str += "\\t"; break;
      case '\\': str += "\\\\"; break;
      case '\"': str += "\\\""; break;
      case '\'': str += "\\\'"; break;
      default: str += ch; break;
      }
    }
    return str;
  }

  std::string to_str() const {
    auto str = interpretation_string(literal);
    // use the length of the (longer) interpretation string.
    return "@" + ident + " = private unnamed_addr constant ["
    + std::to_string(str.length() + 1) + " x i8] c\"" + str + "\\00\", align 1";
  }
};

// ident:
//   lines
struct IRGenerator::BasicBlockPack {
  StringT label;
  std::vector<std::unique_ptr<Instruction>> instructions;

  std::string to_str() const {
    std::string res = label + ":";
    for(auto &instr: instructions)
      res += "\n  " + instr->to_str();
    return res;
  }
};

struct IRGenerator::FunctionPack {
  StringT ident;
  IRType ret_type;
  std::vector<std::pair<StringT, IRType>> params;
  std::vector<BasicBlockPack> basic_block_packs;

  std::string to_str() const {
    std::string res = "define " + ret_type.to_str() + " @" + ident + "(";
    for(int i = 0; i < params.size(); ++i) {
      res += params[i].second.to_str() + " %" + params[i].first;
      if(i != params.size() - 1) res += ", ";
    }
    res += ") {\n";
    for(auto &basic_block: basic_block_packs)
      res += basic_block.to_str() + "\n";
    res += "}";
    return res;
  }
};

struct IRGenerator::IRPack {
  std::vector<TypeDeclarationPack> type_declaration_packs;
  std::vector<StaticPack> static_packs;
  std::vector<FunctionPack> function_packs;

  std::string to_str() const {
    std::string res;
    res += "\n; Writing type declaration packs\n\n";
    for(auto &t: type_declaration_packs) res += t.to_str() + '\n';
    res += "\n; Writing static packs\n\n";
    for(auto &s: static_packs) res += s.to_str() + "\n\n";
    res += "\n; Writing function packs\n\n";
    for(auto &f: function_packs) res += f.to_str() + "\n\n\n";
    return res;
  }
};

struct IRGenerator::FunctionContext {
  bool is_unreachable = false;
  int _next_reg_id = 0, _next_block_id = 0;
  std::vector<BasicBlockPack> basic_block_packs;
  std::vector<std::unique_ptr<Instruction>> instructions;
  // result of node with this node id is in which register
  // break/return result is also stored.
  std::unordered_map<int, int> node_reg_map;
  // which register records the address of variable in memory, and the type of the variable
  std::unordered_map<StringT, std::pair<int, IRType>> variable_addr_reg_map;
  FunctionPack function_pack;

  struct LoopContext {
    std::string jump_label; // cond for while, body for loop
    std::string exit_label; // exit for both while and loop
    int res_ptr_id;
  };
  std::vector<LoopContext> loop_contexts;

  int new_block_id() { return _next_block_id++; }
  int new_reg_id() { return _next_reg_id++; }
  void start_new_block(const std::string &block_label) {
    is_unreachable = false;
    basic_block_packs.back().instructions = std::move(instructions);
    basic_block_packs.emplace_back(BasicBlockPack{.label = block_label});
  }
  template <class Inst> requires std::is_base_of_v<Instruction, Inst>
  void push_instruction(Inst &&inst) {
    if(is_unreachable) return;
    instructions.emplace_back(std::make_unique<Inst>(std::move(inst)));
  }
};

IRGenerator::IRGenerator(stype::TypePool *type_pool)
: _type_pool(type_pool), _ir_pack(std::make_unique<IRPack>()) {}

IRGenerator::~IRGenerator() = default;

std::string IRGenerator::IR_str() const {
  return _ir_pack->to_str();
}

std::string IRGenerator::use_string_literal(StringT literal) {
  if(auto it = _string_literal_pool.find(literal); it != _string_literal_pool.end())
    return it->second;
  std::string ident = ".str" + std::to_string(_string_literal_pool.size());
  _string_literal_pool.emplace(literal, ident);
  _ir_pack->static_packs.push_back(StaticPack{.ident = ident, .literal = literal});
  return ident;
}

void IRGenerator::preVisit(ast::ConstantItem &node) {
  _is_in_const = true;
}

void IRGenerator::postVisit(ast::ConstantItem &node) {
  _is_in_const = false;
}

void IRGenerator::preVisit(ast::Function &node) {
  // a new context
  _contexts.emplace_back();

  // basic block entrance:
  _contexts.back().basic_block_packs.push_back(BasicBlockPack{
    .label = "entry",
  });

  auto info = find_symbol(node.ident());
  auto func_tp = info->type.get<stype::FunctionType>();
  std::vector<std::pair<StringT, IRType>> params;
  // llvm requires register names in pure number style to appear in order.
  // so...
  _contexts.back()._next_reg_id += (func_tp->self_type_opt() ? 1 : 0) + func_tp->params().size();
  int next_param_reg_id = 0;
  if(func_tp->self_type_opt()) {
    int reg_id0 = next_param_reg_id++;
    StringT reg_name0 = std::to_string(reg_id0); // x0

    // self is passed by Self(self) or Self*(&self, &mut self)
    auto ty = IRType(func_tp->self_type_opt());

    // pass as the first parameter
    // Ty* %x0 (self)
    // %x1 = alloca Ty
    // store Ty %x0, Ty* %x1
    // var_ptr-reg-map["self"] = x1, Ty

    int reg_id1 = _contexts.back().new_reg_id();
    auto reg_name1 = std::to_string(reg_id1);

    AllocaInst lineA;
    lineA.dst_name = reg_name1; // x1
    lineA.type = ty;

    _contexts.back().push_instruction(std::move(lineA));

    StoreInst lineS;
    lineS.is_instant = false;
    lineS.value_type = ty;
    lineS.ptr_type = ty.get_ref(_type_pool);
    lineS.value_or_name = reg_name0;
    lineS.ptr_name = reg_name1;
    _contexts.back().push_instruction(std::move(lineS));

    _contexts.back().variable_addr_reg_map.emplace("self", std::pair(reg_id1, ty));

    // Ty %x0 (self)
    params.emplace_back(reg_name0, ty);
  }
  for(int i = 0; i < func_tp->params().size(); ++i) {
    // Ty %x0 (name)
    int reg_id0 = next_param_reg_id++;
    auto param = node.params_opt()->func_params()[i].get();
    StringT reg_name0 = std::to_string(reg_id0); // x0
    if(auto p = dynamic_cast<ast::FunctionParamPattern*>(param)) {
      auto pat = dynamic_cast<ast::IdentifierPattern*>(p->pattern().get());
      if(!pat) {
        throw std::runtime_error("Func param too complicated");
      }
      // allocate in memory.
      // %x1 = alloca Ty
      // store Ty %x0, Ty* %x1
      // var_ptr-reg-map[name] = x1

      auto ty = IRType(func_tp->params()[i]);
      int reg_id1 = _contexts.back().new_reg_id();
      auto reg_name1 = std::to_string(reg_id1);

      AllocaInst lineA;
      lineA.dst_name = reg_name1; // x1
      lineA.type = ty;
      _contexts.back().push_instruction(std::move(lineA));

      StoreInst lineS;
      lineS.is_instant = false;
      lineS.value_type = ty;
      lineS.ptr_type = ty.get_ref(_type_pool);
      lineS.value_or_name = reg_name0;
      lineS.ptr_name = reg_name1;
      _contexts.back().push_instruction(std::move(lineS));

      _contexts.back().variable_addr_reg_map.emplace(pat->ident(), std::pair(reg_id1, ty));
    }
    // Ty %x0 (name)
    params.emplace_back(reg_name0, func_tp->params()[i]);
  }
  _contexts.back().function_pack = FunctionPack{
    .ident = node.ident(),
    .ret_type = IRType(func_tp->ret_type()),
    .params = params
  };
  ScopedVisitor::preVisit(node);
}

void IRGenerator::postVisit(ast::Function &node) {
  ScopedVisitor::postVisit(node);
  // ends. if nothing is explicitly returned, write a "ret void"
  if(node.body_opt()) {
    if(!node.body_opt()->always_returns()) { // check _context.back().is_unreachable?
      ReturnInst lineR;
      lineR.ret_type = IRType(_type_pool->make_unit()); // redundant.
      lineR.ret_val = "";
      _contexts.back().push_instruction(std::move(lineR));
    }
  }
  _contexts.back().basic_block_packs.back().instructions = std::move(_contexts.back().instructions);
  _contexts.back().function_pack.basic_block_packs = std::move(_contexts.back().basic_block_packs);
  _ir_pack->function_packs.push_back(std::move(_contexts.back().function_pack));
  _contexts.pop_back();
}

void IRGenerator::preVisit(ast::StructStruct &node) {
  auto info = find_symbol(node.ident());
  auto struct_tp = info->type.get<stype::StructType>();
  std::vector<IRType> fields;
  for(auto &field: struct_tp->ordered_fields()) {
    fields.emplace_back(field.second);
  }
  _ir_pack->type_declaration_packs.emplace_back(node.ident(), std::move(fields));
}

void IRGenerator::preVisit(ast::ArithmeticOrLogicalExpression &node) {
  // contrary to combine_primes()
  switch(node.oper()) {
  case ast::Operator::kAdd:
  case ast::Operator::kSub:
  case ast::Operator::kMul:
  case ast::Operator::kDiv:
  case ast::Operator::kMod:
  case ast::Operator::kBitwiseAnd:
  case ast::Operator::kBitwiseOr:
  case ast::Operator::kBitwiseXor: {
    auto lhs = node.expr1()->get_type(), rhs = node.expr2()->get_type();
    if(lhs->is_undetermined() && !rhs->is_undetermined()) {
      node.expr1()->set_type(rhs);
    } else if(!lhs->is_undetermined() && rhs->is_undetermined()) {
      node.expr2()->set_type(lhs);
    } else if(lhs->is_undetermined() && rhs->is_undetermined()) {
      auto pl = lhs.get_if<stype::PrimeType>(), pr = rhs.get_if<stype::PrimeType>();
      if(!pl || !pr || pl->prime() != pr->prime()) {
        throw std::runtime_error("invalid undetermined type");
      }
      stype::TypePtr tp;
      if(pl->prime() == stype::TypePrime::kInt) {
        tp = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      } else if(pl->prime() == stype::TypePrime::kFloat) {
        tp = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kF32);
      } else {
        throw std::runtime_error("invalid undetermined type");
      }
      node.expr1()->set_type(tp);
      node.expr2()->set_type(tp);
    }
  } break;
  default: break;
  }
}

void IRGenerator::preVisit(ast::ComparisonExpression &node) {
  auto lhs = node.expr1()->get_type(), rhs = node.expr2()->get_type();
  if(lhs->is_undetermined() && !rhs->is_undetermined()) {
    node.expr1()->set_type(rhs);
  } else if(!lhs->is_undetermined() && rhs->is_undetermined()) {
    node.expr2()->set_type(lhs);
  } else if(lhs->is_undetermined() && rhs->is_undetermined()) {
    auto pl = lhs.get_if<stype::PrimeType>(), pr = rhs.get_if<stype::PrimeType>();
    if(!pl || !pr || pl->prime() != pr->prime()) {
      throw std::runtime_error("invalid undetermined type");
    }
    stype::TypePtr tp;
    if(pl->prime() == stype::TypePrime::kInt) {
      tp = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
    } else if(pl->prime() == stype::TypePrime::kFloat) {
      tp = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kF32);
    } else {
      throw std::runtime_error("invalid undetermined type");
    }
    node.expr1()->set_type(tp);
    node.expr2()->set_type(tp);
  }
}

void IRGenerator::preVisit(ast::CompoundAssignmentExpression &node) {
  // same as AssignmentExpression.
  // contrary to combine_primes()
  switch(node.oper()) {
  case ast::Operator::kAdd:
  case ast::Operator::kSub:
  case ast::Operator::kMul:
  case ast::Operator::kDiv:
  case ast::Operator::kMod:
  case ast::Operator::kBitwiseAnd:
  case ast::Operator::kBitwiseOr:
  case ast::Operator::kBitwiseXor: {
    auto lhs = node.expr1()->get_type(), rhs = node.expr2()->get_type();
    if(lhs->is_undetermined() && !rhs->is_undetermined()) {
      node.expr1()->set_type(rhs);
    } else if(!lhs->is_undetermined() && rhs->is_undetermined()) {
      node.expr2()->set_type(lhs);
    } else if(lhs->is_undetermined() && rhs->is_undetermined()) {
      auto pl = lhs.get_if<stype::PrimeType>(), pr = rhs.get_if<stype::PrimeType>();
      if(!pl || !pr || pl->prime() != pr->prime()) {
        throw std::runtime_error("invalid undetermined type");
      }
      stype::TypePtr tp;
      if(pl->prime() == stype::TypePrime::kInt) {
        tp = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      } else if(pl->prime() == stype::TypePrime::kFloat) {
        tp = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kF32);
      } else {
        throw std::runtime_error("invalid undetermined type");
      }
      node.expr1()->set_type(tp);
      node.expr2()->set_type(tp);
    }
  } break;
  default: break;
  }
}

void IRGenerator::preVisit(ast::LiteralExpression &node) {
  if(node.prime() == stype::TypePrime::kInt) {
    node.set_type(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32));
  } else if(node.prime() == stype::TypePrime::kFloat) {
    node.set_type(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kF32));
  }
}

void IRGenerator::postVisit(ast::LiteralExpression &node) {
  if(_is_in_const) return; // ignore this. already const evaluated.
  if(node.prime() == stype::TypePrime::kInt
    || node.prime() == stype::TypePrime::kFloat
    || node.prime() == stype::TypePrime::kString) {
    throw std::runtime_error("Unrecognized/Impossible literal type");
  }
  // %x0 = alloca Ty
  // store Ty val, Ty* %x0
  auto ty = IRType(node.get_type());
  auto ty_ref = ty.get_ref(_type_pool);
  auto reg0_id = _contexts.back().new_reg_id();
  auto reg0_name = std::to_string(reg0_id);
  AllocaInst lineA;
  lineA.dst_name = reg0_name;
  lineA.type = ty;
  _contexts.back().push_instruction(std::move(lineA));

  StoreInst lineS;
  lineS.is_instant = true;
  lineS.value_type = ty;
  lineS.ptr_type = ty_ref;
  lineS.ptr_name = reg0_name;
  std::visit([&]<typename T0>(T0 &&arg) {
    using T = std::decay_t<T0>;
    if constexpr(std::is_same_v<T, char>) {
      lineS.value_or_name = arg;
    } else if constexpr(std::is_same_v<T, bool>) {
      lineS.value_or_name = arg ? "1" : "0";
    } else if constexpr(std::is_same_v<T, float>) {
      lineS.value_or_name = std::format("{:g}", arg);
    } else if constexpr(std::is_same_v<T, double>) {
      lineS.value_or_name = std::format("{:g}", arg);
    } else if constexpr(std::is_same_v<T, std::string>) {
      // string literal.
      // The normal progress is:
      //
      // %0 = alloca Ty
      // store Ty val, Ty* %0
      // %res = load Ty, Ty* %0
      //
      // For string literal, this progress is:
      //
      // %0 = alloca Ty (i8*, which is exactly the interpretation of RefType(PrimeType(kStr)))
      // %1 = bitcast [N x i8]* @.str-x to Ty
      // store Ty %1, Ty* %0
      // %res = load Ty, Ty* %0
      //
      // exactly a bitcast is added, and store instancy is changed.
      int str_id = _contexts.back().new_reg_id();
      CastInst lineC;
      lineC.must_be_bitcast = true;
      lineC.dst_name = std::to_string(str_id);
      lineC.dst_type = ty;
      lineC.src_type = IRType(_type_pool->make_type<stype::RefType>(
        _type_pool->make_type<stype::ArrayType>(
          _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI8),
          StaticPack::interpretation_string(arg).length() + 1
        ),
        false));
      lineC.is_static_src = true;
      lineC.src_name = use_string_literal(arg);
      _contexts.back().push_instruction(std::move(lineC));

      lineS.is_instant = false;
      lineS.value_or_name = std::to_string(str_id);
    } else if constexpr(std::is_same_v<T, std::int64_t>) {
      lineS.value_or_name = std::to_string(arg);
    } else if constexpr(std::is_same_v<T, std::uint64_t>) {
      lineS.value_or_name = std::to_string(arg);
    } else {
      throw std::runtime_error("Unrecognized type out of primitive container types");
    }
  }, node.spec_value());
  _contexts.back().push_instruction(std::move(lineS));

  int reg1_id = _contexts.back().new_reg_id();
  // %x1 = load Ty, Ty* %x0
  LoadInst lineL;
  lineL.dst_name = std::to_string(reg1_id);
  lineL.load_type = ty;
  lineL.ptr_type = ty_ref;
  lineL.ptr_name = std::to_string(reg0_id);
  _contexts.back().push_instruction(std::move(lineL));

  // record %x1 (the value)
  _contexts.back().node_reg_map.emplace(node.id(), reg1_id);
}

void IRGenerator::postVisit(ast::CallExpression &node) {
  auto func_tp = node.expr()->get_type().get<stype::FunctionType>();
  // %x0 = call ret_t @func(Ty %1, ...)
  // call void @func(Ty %1, ...)
  // no sret

  CallInst lineC;
  lineC.func_name = func_tp->ident();
  lineC.ret_type = IRType(func_tp->ret_type());
  if(func_tp->ret_type() != _type_pool->make_unit()) {
    auto res_id = _contexts.back().new_reg_id();
    lineC.dst_name = std::to_string(res_id);
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
  }

  if(node.params_opt()) for(int i = 0; i < node.params_opt()->expr_list().size(); ++i) {
    auto &expr = node.params_opt()->expr_list()[i];
    auto node_id = expr->id();
    int reg_id = _contexts.back().node_reg_map.at(node_id);
    lineC.args.emplace_back(func_tp->params()[i], std::to_string(reg_id));
  }
  _contexts.back().push_instruction(std::move(lineC));
}

void IRGenerator::postVisit(ast::LetStatement &node) {
  // binding... whatever.
  auto ip = dynamic_cast<ast::IdentifierPattern*>(node.pattern().get());
  if(!ip) { throw std::runtime_error("let pattern too complicated"); }
  auto ty = IRType(
    node.type_opt() ? node.type_opt()->get_type() : node.expr_opt()->get_type()
    ); // type tag first; or something like let a: i32 = 1 (deduced to kInt) might happen. Avoiding it.
  if(!ty) { throw std::runtime_error("No type in let statement"); }
  // %x0 = alloca Ty
  auto reg_id0 = _contexts.back().new_reg_id();
  auto reg_name0 = std::to_string(reg_id0);
  AllocaInst lineA;
  lineA.dst_name = reg_name0;
  lineA.type = ty;
  _contexts.back().push_instruction(std::move(lineA));

  // register address of this variable
  _contexts.back().variable_addr_reg_map.emplace(ip->ident(), std::pair(reg_id0, ty));

  if(node.expr_opt()) {
    // store Ty %x1, Ty* %x0
    auto expr_reg_id = _contexts.back().node_reg_map.at(node.expr_opt()->id());
    auto reg_name1 = std::to_string(expr_reg_id);
    StoreInst lineS;
    lineS.is_instant = false;
    lineS.value_or_name = reg_name1;
    lineS.ptr_name = reg_name0;
    lineS.value_type = ty;
    lineS.ptr_type = ty.get_ref(_type_pool);
    _contexts.back().push_instruction(std::move(lineS));
  }
}

void IRGenerator::postVisit(ast::AssignmentExpression &node) {
  // lhs shall return a pointer Ty*, and rhs shall return a value Ty.
  // Just use one store.
  int lhs_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  int rhs_id = _contexts.back().node_reg_map.at(node.expr2()->id());

  // store Ty %rhs, Ty* %lhs
  StoreInst lineS;
  lineS.is_instant = false;
  lineS.value_or_name = std::to_string(rhs_id);
  lineS.ptr_name = std::to_string(lhs_id);
  lineS.value_type = IRType(node.expr2()->get_type());
  lineS.ptr_type = IRType(node.expr1()->get_type());
  _contexts.back().push_instruction(std::move(lineS));

  // still stands for the rhs_id register.
  _contexts.back().node_reg_map.emplace(node.id(), rhs_id);
}

void IRGenerator::postVisit(ast::BorrowExpression &node) {
  // & val: add a layer of pointer
  // use AllocaInst and StoreInst.
  // %lhs = alloca Ty
  // store Ty %rhs, Ty* %lhs
  // record lhs
  int rhs_id = _contexts.back().node_reg_map.at(node.expr()->id());
  int lhs_id = _contexts.back().new_reg_id();

  AllocaInst lineA;
  lineA.dst_name = std::to_string(lhs_id);
  lineA.type = IRType(node.expr()->get_type());
  _contexts.back().push_instruction(std::move(lineA));

  StoreInst lineS;
  lineS.is_instant = false;
  lineS.value_type = IRType(node.expr()->get_type());
  lineS.ptr_type = IRType(node.get_type());
  lineS.ptr_name = std::to_string(lhs_id);
  lineS.value_or_name = std::to_string(rhs_id);
  _contexts.back().push_instruction(std::move(lineS));

  _contexts.back().node_reg_map.emplace(node.id(), lhs_id);
}

void IRGenerator::postVisit(ast::DereferenceExpression &node) {
  // *ptr: deprive one layer of pointer.
  // %lhs = load Ty, Ty* %rhs
  // record lhs
  int rhs_id = _contexts.back().node_reg_map.at(node.expr()->id());
  int lhs_id = _contexts.back().new_reg_id();

  LoadInst lineL;
  lineL.dst_name = std::to_string(lhs_id);
  lineL.ptr_name = std::to_string(rhs_id);
  lineL.load_type = IRType(node.get_type());
  lineL.ptr_type = IRType(node.expr()->get_type());
  _contexts.back().push_instruction(std::move(lineL));

  _contexts.back().node_reg_map.emplace(node.id(), lhs_id);
}

void IRGenerator::postVisit(ast::PathInExpression &node) {
  // if you found it as a function name, ignore it. needless to be handled here.
  // if you found it as a variable name: grab its value.
  // enum variable... sorry I can't do it.
  if(node.segments().size() != 1) {
    throw std::runtime_error("PathInExpression too complicated");
  }
  auto ident = node.segments().back()->ident_seg()->ident();
  auto info = find_symbol(ident);
  if(info->kind == ast::SymbolKind::kConstant) {
    // value must be inlined. we only support integers here.
    auto value = *info->cval->get_if<sconst::ConstPrime>()->get_usize();
    // record a register that contains this value.
    // %res = add Ty 0, value
    int res_id = _contexts.back().new_reg_id();
    BinaryOpInst lineB;
    lineB.is_l_instant = true;
    lineB.is_r_instant = true;
    lineB.lhs = "0";
    lineB.rhs = std::to_string(value);
    lineB.type = IRType(info->cval->type());
    lineB.dst = std::to_string(res_id);
    lineB.op = "add";
    _contexts.back().push_instruction(std::move(lineB));
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
    return;
  }
  if(info->kind != ast::SymbolKind::kVariable) return;

  auto [val_ptr_id, ty] = _contexts.back().variable_addr_reg_map.at(ident);

  auto here_id = val_ptr_id;
  // lside: record Ty*
  // rside: load, and record Ty
  if(!node.is_lside()) {
    here_id = _contexts.back().new_reg_id();
    // %here = load Ty, Ty* %val_ptr
    LoadInst lineL;
    lineL.dst_name = std::to_string(here_id);
    lineL.ptr_name = std::to_string(val_ptr_id);
    lineL.load_type = ty;
    lineL.ptr_type = ty.get_ref(_type_pool); // symbol table conflict...?
    _contexts.back().push_instruction(std::move(lineL));
  }

  _contexts.back().node_reg_map.emplace(node.id(), here_id);
}

void IRGenerator::postVisit(ast::IndexExpression &node) {
  // input - obj: (&(mut)) [T; N], index: usize.
  // output - obj[index]: T.
  // getelementptr is needed.

  int obj_id = _contexts.back().node_reg_map.at(node.expr_obj()->id());
  int index_id = _contexts.back().node_reg_map.at(node.expr_index()->id());

  int ptr_id = -1; // ArrTy*
  auto arr_ty = IRType(node.expr_obj()->get_type());
  if(!arr_ty.type().get_if<stype::RefType>()) {
    // if given [T; N], then it must be on rside. put it in memory.
    ptr_id = _contexts.back().new_reg_id();
    // store the array into the memory.
    // %ptr = alloca ArrTy
    // store ArrTy %obj, ArrTy* %ptr
    AllocaInst lineA;
    lineA.dst_name = std::to_string(ptr_id);
    lineA.type = arr_ty;
    _contexts.back().push_instruction(std::move(lineA));

    StoreInst lineS;
    lineS.is_instant = false;
    lineS.ptr_type = arr_ty.get_ref(_type_pool);
    lineS.value_type = arr_ty;
    lineS.ptr_name = std::to_string(ptr_id);
    lineS.value_or_name = std::to_string(obj_id);
    _contexts.back().push_instruction(std::move(lineS));
  } else {
    // if given [T; N]* / **, then the address is needed. deref it to [T; N]*.
    ptr_id = obj_id;
    arr_ty = IRType(arr_ty.type().get_if<stype::RefType>()->inner());
    while(true) {
      auto r = arr_ty.type().get_if<stype::RefType>();
      if(!r) break;
      int ptr2_id = _contexts.back().new_reg_id();
      LoadInst lineL;
      lineL.dst_name = std::to_string(ptr2_id);
      lineL.load_type = arr_ty;
      lineL.ptr_name = std::to_string(ptr_id);
      lineL.ptr_type = arr_ty.get_ref(_type_pool);
      _contexts.back().push_instruction(std::move(lineL));
      ptr_id = ptr2_id;
      arr_ty = IRType(r->inner());
    }
  }

  // ptr is ArrTy*, or [T; N]* now.
  int dst_id = _contexts.back().new_reg_id();
  // (T*) %dst = getelementptr [N x T], [N x T]* %ptr, i32 0, index_t %idx
  GEPInst lineG;
  lineG.dst_name = std::to_string(dst_id);
  lineG.base_type = arr_ty;
  lineG.ptr_type = arr_ty.get_ref(_type_pool);
  lineG.ptr_name = std::to_string(ptr_id);
  lineG.index_type = IRType(node.expr_index()->get_type());
  lineG.is_idx_instant = false;
  lineG.index_name_or_value = std::to_string(index_id);
  _contexts.back().push_instruction(std::move(lineG));

  int val_id = dst_id;
  // if it's on lside, we need it to remain &T (Ty*);
  // if it's on rside, we need to load its value (T, Ty).
  if(!node.is_lside()) {
    val_id = _contexts.back().new_reg_id();
    auto ty = IRType(node.get_type());
    // %val = load Ty, Ty* %dst
    LoadInst lineL;
    lineL.dst_name = std::to_string(val_id);
    lineL.load_type = ty;
    lineL.ptr_type = ty.get_ref(_type_pool);
    lineL.ptr_name = std::to_string(dst_id);
    _contexts.back().push_instruction(std::move(lineL));
  }
  // record %val
  _contexts.back().node_reg_map.emplace(node.id(), val_id);
}

void IRGenerator::postVisit(ast::TypeCastExpression &node) {
  // %c = bitcast/zext/sext/trunc Ty1* %p to Ty2*
  auto ty1 = IRType(node.expr()->get_type());
  auto ty2 = IRType(node.type_no_bounds()->get_type());
  auto src_id = _contexts.back().node_reg_map.at(node.expr()->id());

  int dst_id = _contexts.back().new_reg_id();
  CastInst lineC;
  lineC.must_be_bitcast = false;
  lineC.is_static_src = false;
  lineC.dst_name = std::to_string(dst_id);
  lineC.dst_type = ty2;
  lineC.src_name = std::to_string(src_id);
  lineC.src_type = ty1;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::postVisit(ast::NegationExpression &node) {
  // unary operation: LogicalNot and ArithmeticInverse
  int val_id = _contexts.back().node_reg_map.at(node.expr()->id());
  int dst_id = _contexts.back().new_reg_id();
  if(node.oper() == ast::Operator::kLogicalNot) {
    // !val -> val == 0
    // %dst = icmp eq Ty %val, 0
    BinaryOpInst lineB;
    lineB.is_l_instant = false;
    lineB.lhs = std::to_string(val_id);
    lineB.is_r_instant = true;
    lineB.rhs = "0";
    lineB.type = IRType(node.expr()->get_type());
    lineB.dst = std::to_string(dst_id);
    lineB.op = "icmp eq";
    _contexts.back().push_instruction(std::move(lineB));

  } else if(node.oper() == ast::Operator::kSub) {
    // -val -> 0 - val
    // %dst = sub Ty 0, %val
    BinaryOpInst lineB;
    lineB.is_l_instant = true;
    lineB.lhs = "0";
    lineB.is_r_instant = false;
    lineB.rhs = std::to_string(val_id);
    lineB.type = IRType(node.expr()->get_type());
    lineB.dst = std::to_string(dst_id);
    lineB.op = "sub";
    _contexts.back().push_instruction(std::move(lineB));
  }
  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::postVisit(ast::ArithmeticOrLogicalExpression &node) {
  // binary operation:
  // %dst = icmp op Ty %val1, %val2
  int val1_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  int val2_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  int dst_id = _contexts.back().new_reg_id();

  BinaryOpInst lineB;
  lineB.is_l_instant = false;
  lineB.lhs = std::to_string(val1_id);
  lineB.is_r_instant = false;
  lineB.rhs = std::to_string(val2_id);
  lineB.type = IRType(node.get_type());
  lineB.dst = std::to_string(dst_id);

  if(!node.expr1()->get_type().get_if<stype::PrimeType>()
    || !node.expr2()->get_type().get_if<stype::PrimeType>()) {
    throw std::runtime_error("Operands too complicated in Arithmetic/Logical Expression");
  }
  bool is_unsigned = node.expr1()->get_type().get<stype::PrimeType>()->is_unsigned_int();
  switch(node.oper()) {
  case ast::Operator::kShl: {
    lineB.op = "shl";
  } break;
  case ast::Operator::kShr: {
    lineB.op = is_unsigned ? "lshr" : "ashr";
  } break;
  case ast::Operator::kAdd: {
    lineB.op = "add";
  } break;
  case ast::Operator::kSub: {
    lineB.op = "sub";
  } break;
  case ast::Operator::kMul: {
    lineB.op = "mul";
  } break;
  case ast::Operator::kDiv: {
    lineB.op = is_unsigned ? "udiv" : "sdiv";
  } break;
  case ast::Operator::kMod: {
    lineB.op = is_unsigned ? "urem" : "srem";
  } break;
  case ast::Operator::kBitwiseAnd: {
    lineB.op = "and";
  } break;
  case ast::Operator::kBitwiseOr: {
    lineB.op = "or";
  } break;
  case ast::Operator::kBitwiseXor: {
    lineB.op = "xor";
  } break;
  default:
    throw std::runtime_error("Invalid operator type in arithmetic/logical expression");
  }

  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::postVisit(ast::ComparisonExpression &node) {
  // binary operation:
  // %dst = op Ty %val1, %val2
  int val1_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  int val2_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  int dst_id = _contexts.back().new_reg_id();

  BinaryOpInst lineB;
  lineB.is_l_instant = false;
  lineB.lhs = std::to_string(val1_id);
  lineB.is_r_instant = false;
  lineB.rhs = std::to_string(val2_id);
  lineB.type = IRType(node.expr1()->get_type()); // operand type, not result type
  lineB.dst = std::to_string(dst_id);

  bool is_unsigned = false;
  if(auto ptr = node.expr1()->get_type().get_if<stype::PrimeType>()) {
    is_unsigned = ptr->is_unsigned_int();
  }

  switch(node.oper()) {
  case ast::Operator::kEq: {
    lineB.op = "icmp eq";
  } break;
  case ast::Operator::kNe: {
    lineB.op = "icmp ne";
  } break;
  case ast::Operator::kLe: {
    lineB.op = is_unsigned ? "icmp ule" : "icmp sle";
  } break;
  case ast::Operator::kLt: {
    lineB.op = is_unsigned ? "icmp ult" : "icmp slt";
  } break;
  case ast::Operator::kGe: {
    lineB.op = is_unsigned ? "icmp uge" : "icmp sge";
  } break;
  case ast::Operator::kGt: {
    lineB.op = is_unsigned ? "icmp ugt" : "icmp sgt";
  } break;
  default:
    throw std::runtime_error("Invalid operator type in arithmetic/logical expression");
  }
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::postVisit(ast::CompoundAssignmentExpression &node) {
  int ptr1_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  int val2_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  // a x= b -> _ = a x b, a = _.
  // a: Ty*, b: Ty.
  auto ty = IRType(node.expr2()->get_type());
  auto ty_ref = ty.get_ref(_type_pool);

  int val1_id = _contexts.back().new_reg_id();
  // %val1 = load Ty, Ty* %ptr1
  LoadInst lineL;
  lineL.dst_name = std::to_string(val1_id);
  lineL.load_type = ty;
  lineL.ptr_type = ty_ref;
  lineL.ptr_name = std::to_string(ptr1_id);
  _contexts.back().push_instruction(std::move(lineL));

  // %res = icmp op Ty %val1, %val2
  int res_id = _contexts.back().new_reg_id();

  BinaryOpInst lineB;
  lineB.is_l_instant = false;
  lineB.lhs = std::to_string(val1_id);
  lineB.is_r_instant = false;
  lineB.rhs = std::to_string(val2_id);
  lineB.type = ty;
  lineB.dst = std::to_string(res_id);

  if(!node.expr1()->get_type().get_if<stype::PrimeType>()
    || !node.expr2()->get_type().get_if<stype::PrimeType>()) {
    throw std::runtime_error("Operands too complicated in Arithmetic/Logical Expression");
    }
  bool is_unsigned = node.expr1()->get_type().get<stype::PrimeType>()->is_unsigned_int();
  switch(node.oper()) {
  case ast::Operator::kShl: {
    lineB.op = "shl";
  } break;
  case ast::Operator::kShr: {
    lineB.op = is_unsigned ? "lshr" : "ashr";
  } break;
  case ast::Operator::kAdd: {
    lineB.op = "add";
  } break;
  case ast::Operator::kSub: {
    lineB.op = "sub";
  } break;
  case ast::Operator::kMul: {
    lineB.op = "mul";
  } break;
  case ast::Operator::kDiv: {
    lineB.op = is_unsigned ? "udiv" : "sdiv";
  } break;
  case ast::Operator::kMod: {
    lineB.op = is_unsigned ? "urem" : "srem";
  } break;
  case ast::Operator::kBitwiseAnd: {
    lineB.op = "and";
  } break;
  case ast::Operator::kBitwiseOr: {
    lineB.op = "or";
  } break;
  case ast::Operator::kBitwiseXor: {
    lineB.op = "xor";
  } break;
  case ast::Operator::kLogicalAnd:
  case ast::Operator::kLogicalOr: {
    throw std::runtime_error("Logical And/Or unimplemented");
  } break;
  default:
    throw std::runtime_error("Invalid operator type in arithmetic/logical expression");
  }
  _contexts.back().push_instruction(std::move(lineB));

  // store Ty %res, Ty* %ptr1
  StoreInst lineS;
  lineS.is_instant = false;
  lineS.value_or_name = std::to_string(res_id);
  lineS.ptr_name = std::to_string(ptr1_id);
  lineS.value_type = ty;
  lineS.ptr_type = ty_ref;
  _contexts.back().push_instruction(std::move(lineS));

  // record nothing. It should return a unit (void)
  // _contexts.back().node_reg_map.emplace(node.id(), res_id);
}
void IRGenerator::postVisit(ast::FieldExpression &node) {
  auto obj_id = _contexts.back().node_reg_map.at(node.expr()->id());
  auto node_tp = node.expr()->get_type();
  // auto deref
  while(node_tp.get_if<stype::RefType>()) {
    node_tp = node_tp.get<stype::RefType>()->inner();
    int deref_id = _contexts.back().new_reg_id();
    LoadInst lineL;
    lineL.load_type = IRType(node_tp);
    lineL.ptr_type = IRType(node_tp).get_ref(_type_pool);
    lineL.ptr_name = std::to_string(obj_id);
    lineL.dst_name = std::to_string(deref_id);
    _contexts.back().push_instruction(std::move(lineL));
    obj_id = deref_id;
  }
  auto struct_ptr = node_tp.get_if<stype::StructType>();
  if(!struct_ptr) {
    throw std::runtime_error("Field expression on non-struct type not detected");
  }
  auto [diff_idx, field_ty] = struct_ptr->field_orders().at(node.ident());
  auto ty = IRType(node_tp);
  auto ty_ref = ty.get_ref(_type_pool);

  int ptr_id = _contexts.back().new_reg_id();
  // %ptr = alloca Ty
  // store Ty %obj, Ty* %ptr
  AllocaInst lineA;
  lineA.dst_name = std::to_string(ptr_id);
  lineA.type = ty;
  _contexts.back().push_instruction(std::move(lineA));

  StoreInst lineS;
  lineS.ptr_type = ty_ref;
  lineS.value_type = ty;
  lineS.ptr_name = std::to_string(ptr_id);
  lineS.is_instant = false;
  lineS.value_or_name = std::to_string(obj_id);
  _contexts.back().push_instruction(std::move(lineS));

  int rptr_id = _contexts.back().new_reg_id();
  // %rptr = getelementptr STy, STy* %ptr, i32 0, usize diff_idx
  GEPInst lineG;
  lineG.dst_name = std::to_string(rptr_id);
  lineG.base_type = IRType(ty);
  lineG.ptr_type = ty_ref;
  lineG.ptr_name = std::to_string(ptr_id);
  lineG.is_idx_instant = true;
  lineG.index_type = IRType(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kUSize));
  lineG.index_name_or_value = std::to_string(diff_idx);
  _contexts.back().push_instruction(std::move(lineG));

  // if on lside, record the pointer rptr.
  // if on rside, record a value res.
  int res_id = rptr_id;
  if(!node.is_lside()) {
    res_id = _contexts.back().new_reg_id();
    // %res = load FieldT, FieldT* %rptr
    LoadInst lineL;
    lineL.dst_name = std::to_string(res_id);
    lineL.load_type = IRType(field_ty);
    lineL.ptr_type = IRType(field_ty).get_ref(_type_pool);
    lineL.ptr_name = std::to_string(rptr_id);
    _contexts.back().push_instruction(std::move(lineL));
  }
  // record %res
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::postVisit(ast::GroupedExpression &node) {
  int res_id = _contexts.back().node_reg_map.at(node.expr()->id());
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::postVisit(ast::StructExpression &node) {
  InsertValueInst lineI;

  auto struct_ty = node.get_type();
  lineI.type = IRType(struct_ty);

  auto stp = struct_ty.get_if<stype::StructType>();
  int field_num = stp->fields().size();
  lineI.interval_regs.reserve(field_num + 1);
  int res_id = -1;
  for(int i = 0; i < field_num; ++i) {
    res_id = _contexts.back().new_reg_id();
    lineI.interval_regs.push_back(std::to_string(res_id));
  }

  // order of field in the struct, field_ty, its register id
  std::vector<std::pair<int, std::pair<IRType, int>>> records;
  if(node.fields_opt()) for(auto &sefp: node.fields_opt()->fields()) {
    if(!sefp->is_named()) {
      throw std::runtime_error("Unsupported index struct field");
    }
    auto ptr = dynamic_cast<ast::NamedStructExprField*>(sefp.get());
    auto ident = ptr->ident();
    auto [order, ty] = stp->field_orders().at(ident);
    auto reg_id = _contexts.back().node_reg_map.at(ptr->expr()->id());
    records.emplace_back(order, std::pair(IRType(ty), reg_id));
  }
  std::sort(records.begin(), records.end(),
    [](const std::pair<int, std::pair<IRType, int>> &A,
      const std::pair<int, std::pair<IRType, int>> &B) {
      return A.first < B.first;
    });
  for(auto &[order, p]: records) {
    lineI.infos.emplace_back(p.first, false, std::to_string(p.second));
  }

  _contexts.back().push_instruction(std::move(lineI));

  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::postVisit(ast::ArrayExpression &node) {
  InsertValueInst lineI;
  lineI.type = IRType(node.get_type());
  int res_id = -1;
  if(!node.elements_opt()) {
    res_id = _contexts.back().new_reg_id();
    lineI.interval_regs.push_back(std::to_string(res_id));
  } else if(node.elements_opt()->is_explicit()) {
    auto eptr = dynamic_cast<ast::ExplicitArrayElements*>(node.elements_opt().get());
    for(int i = 0; i < eptr->expr_list().size(); ++i) {
      res_id = _contexts.back().new_reg_id();
      lineI.interval_regs.push_back(std::to_string(res_id));
    }
    for(int i = 0; i < eptr->expr_list().size(); ++i) {
      auto expr_id = _contexts.back().node_reg_map.at(eptr->expr_list()[i]->id());
      lineI.infos.emplace_back(IRType(eptr->expr_list()[i]->get_type()), false, std::to_string(expr_id));
    }
  } else {
    auto rptr = dynamic_cast<ast::RepeatedArrayElements*>(node.elements_opt().get());
    auto length = rptr->len_expr()->cval()->get<sconst::ConstPrime>().get_usize();
    for(int i = 0; i < length; ++i) {
      res_id = _contexts.back().new_reg_id();
      lineI.interval_regs.push_back(std::to_string(res_id));
    }
    auto expr_id = _contexts.back().node_reg_map.at(rptr->val_expr()->id());
    for(int i = 0; i < length; ++i) {
      lineI.infos.emplace_back(IRType(rptr->val_expr()->get_type()), false, std::to_string(expr_id));
    }
  }
  _contexts.back().push_instruction(std::move(lineI));

  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::postVisit(ast::BlockExpression &node) {
  ScopedVisitor::postVisit(node);
  if(node.stmts_opt() && node.stmts_opt()->expr_opt()) {
    auto node_id = node.stmts_opt()->expr_opt()->id();
    auto reg_id = _contexts.back().node_reg_map.at(node_id);
    _contexts.back().node_reg_map.emplace(node.id(), reg_id);
  }
  // if it has no value, do not register anything into node-reg-map.
}

void IRGenerator::postVisit(ast::Conditions &node) {
  auto res_id = _contexts.back().node_reg_map.at(node.expr()->id());
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::visit(ast::IfExpression &node) {
  // if (cond) { block } else { block (normal / non exist / another if) }
  // label: if.then, if.else, if.exit

  //   #cond...
  //   br i1 %cond_res, label %if.then, label %if.else
  // if.then:
  //   #body
  //   %then_res = ...
  //   br label %if.exit
  // if.else:
  // (if there is else)
  //   #else content
  //   %else_res = ...
  //   br label $if.exit
  // (\if there is else)
  // if.exit:
  //   %if_res = %then_res / #else_res
  // (if there is else)
  //   %if_res = phi Ty [%then_res, %if.then], [%else_res, $if.else]
  //   (go on)

  int label_id = _contexts.back().new_block_id();
  std::string then_label = "if.then." + std::to_string(label_id);
  std::string else_label = "if.else." + std::to_string(label_id);
  std::string exit_label = "if.exit." + std::to_string(label_id);

  node.cond()->accept(*this);

  auto cond_id = _contexts.back().node_reg_map.at(node.cond()->id());
  CondBranchInst lineC;
  lineC.cond_name = std::to_string(cond_id);
  lineC.true_label = then_label;
  lineC.false_label = else_label;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().start_new_block(then_label);

  node.block_expr()->accept(*this);

  // then res
  int then_id = -1;
  if(auto it = _contexts.back().node_reg_map.find(node.block_expr()->id());
    it != _contexts.back().node_reg_map.end()) {
    then_id = it->second;
  }

  BranchInst lineB;
  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(else_label);

  int else_id = -1;

  std::visit([&]<class T0>(const T0& arg) {
    using T = std::decay_t<T0>; // ...
    if constexpr(!std::is_same_v<T, std::monostate>) {
      arg->accept(*this);
      if(auto it = _contexts.back().node_reg_map.find(arg->id());
        it != _contexts.back().node_reg_map.end()) {
        else_id = it->second;
      }
    }
  }, node.else_spec());

  // reuse lineB
  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(exit_label);

  if(then_id == -1 && else_id == -1) {
    // No need to do anything.
    // nothing to record.
    return;
  }
  int exit_id = _contexts.back().new_block_id();
  if(then_id != -1 && else_id != -1) {
    PhiInst lineP;
    lineP.dst_name = std::to_string(exit_id);
    lineP.dst_type = IRType(node.get_type());
    lineP.is_res1_instant = false;
    lineP.res1_name_or_value = std::to_string(then_id);
    lineP.label1 = then_label;
    lineP.is_res2_instant = false;
    lineP.res2_name_or_value = std::to_string(else_id);
    lineP.label2 = else_label;
    _contexts.back().push_instruction(std::move(lineP));
  } else if(then_id != -1) {
    exit_id = then_id;
  } else {
    exit_id = else_id;
  }
  _contexts.back().node_reg_map.emplace(node.id(), exit_id);
}

void IRGenerator::visit(ast::PredicateLoopExpression &node) {
  // while (cond) { block }
  // label: while.cond, while.body, while.exit

  //   br label while.cond
  // while.cond:
  //   #cond...
  //   br i1 %cond_res, label %while.body, label %while.exit
  // while.body:
  //   ...
  //   (end of body) br label while.cond
  // while.exit:
  //   ...
  int label_id = _contexts.back().new_block_id();
  std::string cond_label = "while.cond." + std::to_string(label_id);
  std::string body_label = "while.body." + std::to_string(label_id);
  std::string exit_label = "while.exit." + std::to_string(label_id);

  BranchInst lineB;
  lineB.label = cond_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(cond_label);

  node.cond()->accept(*this);
  int cond_id = _contexts.back().node_reg_map.at(node.cond()->id());

  _contexts.back().loop_contexts.emplace_back(cond_label, exit_label, -1);

  CondBranchInst lineC;
  lineC.cond_name = std::to_string(cond_id);
  lineC.true_label = body_label;
  lineC.false_label = exit_label;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().start_new_block(body_label);

  node.block_expr()->accept(*this);

  lineB.label = cond_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(exit_label);

  _contexts.back().loop_contexts.pop_back();
}

void IRGenerator::visit(ast::InfiniteLoopExpression &node) {
  // loop { block }
  // label: loop.body, loop.exit

  auto ty = IRType(node.get_type());
  bool not_void = !ty.is_void(_type_pool);

  //   (if not_void) %res_ptr = alloca Ty
  //   br label loop.body
  // loop.body:
  //   ...
  //   (break) store Ty %value, Ty* res_ptr
  //   br label loop.exit
  //   ...
  //   (end of body) br label loop.body
  // loop.exit:
  //   (if not_void) %res = load Ty, Ty* %res_ptr

  int label_id = _contexts.back().new_block_id();
  std::string body_label = "loop.body." + std::to_string(label_id);
  std::string exit_label = "loop.exit." + std::to_string(label_id);

  int res_ptr_id = -1;
  if(not_void) {
    res_ptr_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.dst_name = std::to_string(res_ptr_id);
    lineA.type = ty;
    _contexts.back().push_instruction(std::move(lineA));
  }
  BranchInst lineB;
  lineB.label = body_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(body_label);

  _contexts.back().loop_contexts.emplace_back(body_label, exit_label, res_ptr_id);

  node.block_expr()->accept(*this);

  lineB.label = body_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(exit_label);

  if(not_void) {
    int res_id = _contexts.back().new_reg_id();
    LoadInst lineL;
    lineL.dst_name = std::to_string(res_id);
    lineL.ptr_name = std::to_string(res_ptr_id);
    lineL.load_type = ty;
    lineL.ptr_type = ty.get_ref(_type_pool);
    _contexts.back().push_instruction(std::move(lineL));
    // record res
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
  }

  _contexts.back().loop_contexts.pop_back();
}

void IRGenerator::postVisit(ast::BreakExpression &node) {
  if(node.expr_opt()) {
    // break value;
    int value_id = _contexts.back().node_reg_map.at(node.expr_opt()->id());
    auto ty = IRType(node.expr_opt()->get_type());
    // must be InfiniteLoop, and res_ptr_id is not -1.
    int res_ptr_id = _contexts.back().loop_contexts.back().res_ptr_id;
    // store Ty &value, Ty* %res_ptr
    StoreInst lineS;
    lineS.is_instant = false;
    lineS.value_or_name = std::to_string(value_id);
    lineS.value_type = ty;
    lineS.ptr_name = std::to_string(res_ptr_id);
    lineS.ptr_type = ty.get_ref(_type_pool);
    _contexts.back().push_instruction(std::move(lineS));
  }
  // break to exit.
  BranchInst lineB;
  lineB.label = _contexts.back().loop_contexts.back().exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().is_unreachable = true;
}

void IRGenerator::postVisit(ast::ContinueExpression &node) {
  // continue to jump label.
  BranchInst lineB;
  lineB.label = _contexts.back().loop_contexts.back().jump_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().is_unreachable = true;
}

void IRGenerator::postVisit(ast::ReturnExpression &node) {
  // just return.
  // no need to record any register.
  ReturnInst lineR;
  if(!node.expr_opt()) {
    lineR.ret_val = "";
  } else {
    lineR.ret_type = IRType(node.expr_opt()->get_type());
    lineR.ret_val = std::to_string(_contexts.back().node_reg_map.at(node.expr_opt()->id()));
  }
  _contexts.back().push_instruction(std::move(lineR));
  _contexts.back().is_unreachable = true;
}

void IRGenerator::visit(ast::LazyBooleanExpression &node) {
  // kLogicalAnd/kLogicalOr

  if(node.oper() != ast::Operator::kLogicalAnd && node.oper() != ast::Operator::kLogicalOr) {
    throw std::runtime_error("Unrecognized operator in LazyBooleanExpression");
  }

  // res = lhs && rhs ->
  // res = if (lhs) { rhs } else { false }

  //   ...(lhs ready, rhs not ready)
  //   br i1 %lhs, label %laz.then, label %lazy_bool.else
  // %laz.then:
  //   (calc rhs here)
  //   br label %laz.exit
  // %laz.else:
  //   br label %laz.exit
  // %laz.exit:
  //   %res = phi i1 [%rhs, %laz.then], [0, %laz.else]

  // res = lhs || rhs ->
  // res = if (lhs) { true } else { rhs }

  //   ...(lhs ready, rhs not ready)
  //   br i1 %lhs, label %laz.then, label %lazy_bool.else
  // %laz.then:
  //   br label %laz.exit
  // %laz.else:
  //   (calc rhs here)
  //   br label %laz.exit
  // %laz.exit:
  //   %res = phi i1 [1, %laz.then], [%rhs, %laz.else]

  node.expr1()->accept(*this);
  int lhs_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  int rhs_id = -1;

  int label_id = _contexts.back().new_block_id();
  std::string then_label = "laz.then." + std::to_string(label_id);
  std::string else_label = "laz.else." + std::to_string(label_id);
  std::string exit_label = "laz.exit." + std::to_string(label_id);

  CondBranchInst lineC;
  lineC.cond_name = std::to_string(lhs_id);
  lineC.true_label = then_label;
  lineC.false_label = else_label;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().start_new_block(then_label);

  if(node.oper() == ast::Operator::kLogicalAnd) {
    node.expr2()->accept(*this);
    rhs_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  }

  BranchInst lineB;
  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(else_label);

  if(node.oper() == ast::Operator::kLogicalOr) {
    node.expr2()->accept(*this);
    rhs_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  }

  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().start_new_block(exit_label);

  int res_id = _contexts.back().new_reg_id();
  PhiInst lineP;
  lineP.dst_name = std::to_string(res_id);
  lineP.dst_type = IRType(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool));
  lineP.label1 = then_label;
  lineP.label2 = else_label;

  switch(node.oper()) {
  case ast::Operator::kLogicalAnd: {
    // %res = phi i1 [%rhs, %laz.then], [0, %laz.else]
    lineP.is_res1_instant = false;
    lineP.res1_name_or_value = std::to_string(rhs_id);
    lineP.is_res2_instant = true;
    lineP.res2_name_or_value = "i1 0";
  } break;
  case ast::Operator::kLogicalOr: {
    // %res = phi i1 [1, %laz.then], [%rhs, %laz.else]
    lineP.is_res1_instant = true;
    lineP.res1_name_or_value = "i1 1";
    lineP.is_res2_instant = false;
    lineP.res2_name_or_value = std::to_string(rhs_id);
  } break;
  default: throw std::runtime_error("Unrecognized operator in LazyBooleanExpression");
  }

  _contexts.back().push_instruction(std::move(lineP));

  // record res
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}


}

















