#include "ir_generator.h"

#include <algorithm>
#include <format>
#include "ir_instruction.h"

namespace rshard::ir {

std::string IRGenerator::use_string_literal(StringT literal) {
  if(auto it = _string_literal_pool.find(literal); it != _string_literal_pool.end())
    return it->second;
  std::string ident = ".str" + std::to_string(_string_literal_pool.size());
  _string_literal_pool.emplace(literal, ident);
  _ir_pack.static_packs.push_back(StaticPack{.ident = ident, .literal = literal});
  return ident;
}

int IRGenerator::store_into_memory(int obj_id, IRType obj_ty) {
  auto ptr_id = _contexts.back().new_reg_id();
  AllocaInst lineA;
  lineA.dst = ptr_id;
  lineA.type = obj_ty;
  _contexts.back().push_instruction(std::move(lineA));
  // _contexts.back().add_reg_hint_from(ptr_id, obj_id, "addr");
  StoreInst lineS;
  lineS.ptr = Operand::make_reg(ptr_id, wrap_ref(obj_ty));
  lineS.value = Operand::make_reg(obj_id, obj_ty);
  _contexts.back().push_instruction(std::move(lineS));
  return ptr_id;
}

int IRGenerator::load_from_memory(int ptr_id, IRType obj_ty) {
  auto obj_id = _contexts.back().new_reg_id();
  LoadInst lineL;
  lineL.dst = obj_id;
  lineL.ptr = Operand::make_reg(ptr_id, wrap_ref(obj_ty));
  lineL.load_type = obj_ty;
  _contexts.back().push_instruction(std::move(lineL));
  // _contexts.back().add_reg_hint_from(obj_id, ptr_id, "value");
  return obj_id;
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

  // add a new map layer
  _contexts.back().variable_addr_reg_maps.emplace_back();

  // basic block entrance:
  Label entry_label(LabelHint::kEntry, _contexts.back().new_hint_tag_id());
  entry_label.label_id = 0;
  _contexts.back().basic_block_packs.push_back(BasicBlockPack{
    .label = entry_label
  });

  auto info = find_symbol(node.ident());
  auto func_tp = info->type.get<stype::FunctionType>();

  std::optional<std::pair<StringT, IRType>> sret_param;
  std::vector<std::pair<StringT, IRType>> params;

  /* fail?
  if(func_tp->impl_type_opt() != _impl_type) {
    throw std::runtime_error("Unexpected impl type");
  }
  */

  // global function name is its original name.
  // method func name shall be mangled by its associated type's type hash.
  std::string inner_func_ident = func_tp->ident();
  if(func_tp->impl_type_opt()) {
    inner_func_ident = mangle_method(func_tp->impl_type_opt(), inner_func_ident);
  }
  // llvm requires register names in pure number style to appear in order.
  // so...
  _contexts.back()._next_reg_id +=
    (func_tp->need_sret() ? 1 : 0) + (func_tp->self_type_opt() ? 1 : 0) + static_cast<int>(func_tp->params().size());
  auto next_param_reg_id = 0;
  auto sret_id = -1;
  if(func_tp->need_sret()) {
    sret_id = next_param_reg_id++;
    sret_param.emplace(std::to_string(sret_id), wrap_ref(IRType(func_tp->ret_type())));
  }
  if(func_tp->self_type_opt()) {
    auto self_id = next_param_reg_id++;

    // self is passed by Self(self) or Self*(&self, &mut self)
    auto ty = IRType(func_tp->self_type_opt());

    // pass as the first parameter
    // Ty* %x0 (self)
    // %x1 = alloca Ty
    // store Ty %x0, Ty* %x1
    // var_ptr-reg-map["self"] = x1, Ty
    auto reg_id1 = store_into_memory(self_id, ty);

    _contexts.back().variable_addr_reg_maps.back().emplace("self", std::pair(reg_id1, ty));

    // Ty %x0 (self)
    params.emplace_back(std::to_string(self_id), ty);
  }
  for(int i = 0; i < func_tp->params().size(); ++i) {
    // Ty %x0 (name)
    auto arg_id = next_param_reg_id++;
    auto param = node.params_opt()->func_params()[i].get();
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
      auto reg_id1 = store_into_memory(arg_id, ty);

      _contexts.back().variable_addr_reg_maps.back().emplace(pat->ident(), std::pair(reg_id1, ty));
    }
    // Ty %x0 (name)
    params.emplace_back(std::to_string(arg_id), func_tp->params()[i]);
  }
  _contexts.back().function_pack = FunctionPack{
    .ident = inner_func_ident,
    .sret_param = sret_param,
    .ret_type = IRType(func_tp->ret_type()),
    .params = params
  };
  ScopedVisitor::preVisit(node);

  if(func_tp->need_sret()) {
    // sret case 1: tail expr as StructExpression
    if(auto se = dynamic_cast<ast::StructExpression*>(node.body_opt()->stmts_opt()->expr_opt().get())) {
      _contexts.back().in_place_node_ptr_map.emplace(se->id(), sret_id);
      se->set_addr_needed();
    }
    // sret case 2: tail expr as PathInExpression
    if(auto pe = dynamic_cast<ast::PathInExpression*>(node.body_opt()->stmts_opt()->expr_opt().get())) {
      // find where you define this variable.
      if(pe->segments().size() != 1) throw std::runtime_error("Can't solve this situation (sret)");
      auto ident = pe->segments().back()->ident_seg()->ident();
      auto info = find_symbol(ident);
      if(!info || info->kind != ast::SymbolKind::kVariable) {
        throw std::runtime_error("Can't solve this situation (sret)");
      }
      // just to be required the last definition... No need worrying about variable shadowing.
      _contexts.back().sret_target = std::pair(ident, sret_id);
      pe->set_addr_needed();
    }
  }
}

void IRGenerator::postVisit(ast::Function &node) {
  ScopedVisitor::postVisit(node);
  if(!node.body_opt()) {
    throw std::runtime_error("Function with no func body not supported");
  }
  if(node.body_opt()->always_returns()) {
    UnreachableInst lineU;
    _contexts.back().push_instruction(std::move(lineU));
  } else {
    // reaches the end of the function. Return something.
    ReturnInst lineR;
    auto info = find_symbol(node.ident());
    auto func_tp = info->type.get<stype::FunctionType>();
    if(func_tp->need_sret() || !node.body_opt()->stmts_opt() || !node.body_opt()->stmts_opt()->expr_opt() ||
      !_contexts.back().node_reg_map.contains(node.body_opt()->stmts_opt()->expr_opt()->id())) { // so amusing a line
      // if nothing is explicitly returned, write a "ret void".
      // check _context.back().is_unreachable?
      lineR.ret_val = std::nullopt;
    } else {
      // return what's left.
      auto reg_id = _contexts.back().node_reg_map.at(node.body_opt()->stmts_opt()->expr_opt()->id());
      lineR.ret_val = Operand::make_reg(reg_id, _contexts.back().function_pack.ret_type);
    }
    _contexts.back().push_instruction(std::move(lineR));
  }
  _contexts.back().basic_block_packs.back().instructions = std::move(_contexts.back().instructions);
  _contexts.back().function_pack.basic_block_packs = std::move(_contexts.back().basic_block_packs);

  _contexts.back().function_pack.fill_label_ids();
  _ir_pack.function_packs.push_back(std::move(_contexts.back().function_pack));

  _contexts.back().variable_addr_reg_maps.pop_back();
  if(!_contexts.back().variable_addr_reg_maps.empty()) {
    throw std::runtime_error("Variable-addr-reg map scope management error");
  }
  _contexts.pop_back();
}

void IRGenerator::preVisit(ast::StructStruct &node) {
  auto info = find_symbol(node.ident());
  auto struct_tp = info->type.get<stype::StructType>();
  std::vector<IRType> fields;
  for(auto &field: struct_tp->ordered_fields()) {
    fields.emplace_back(field.second);
  }
  _ir_pack.type_declaration_packs.emplace_back(node.ident(), std::move(fields));
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
        tp = _type_pool->make_i32();
      } else if(pl->prime() == stype::TypePrime::kFloat) {
        tp = _type_pool->make_f32();
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
      tp = _type_pool->make_i32();
    } else if(pl->prime() == stype::TypePrime::kFloat) {
      tp = _type_pool->make_f32();
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
        tp = _type_pool->make_i32();
      } else if(pl->prime() == stype::TypePrime::kFloat) {
        tp = _type_pool->make_f32();
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
    node.set_type(_type_pool->make_i32());
  } else if(node.prime() == stype::TypePrime::kFloat) {
    node.set_type(_type_pool->make_f32());
  }
}

void IRGenerator::postVisit(ast::LiteralExpression &node) {
  if(_is_in_const) return; // ignore this. already const evaluated.
  if(node.prime() == stype::TypePrime::kInt
    || node.prime() == stype::TypePrime::kFloat
    || node.prime() == stype::TypePrime::kString) {
    throw std::runtime_error("Unrecognized/Impossible literal type");
    }

  auto ty = IRType(node.get_type());
  auto val_id = _contexts.back().new_reg_id();
  BinaryOpInst lineB;
  lineB.lhs = Operand::make_imm(0, ty);
  lineB.dst = val_id;
  lineB.op = "add";
  lineB.type = ty;
  std::string hint = "lit-";
  std::visit([&]<typename T0>(T0 &&arg) {
    using T = std::decay_t<T0>;
    if constexpr(std::is_same_v<T, bool>) {
      lineB.rhs = Operand::make_imm(arg ? 1 : 0, ty);
      hint += arg ? "true" : "false";
    } else if constexpr(std::is_same_v<T, std::int64_t> || std::is_same_v<T, std::uint64_t>) {
      lineB.rhs = Operand::make_imm(arg, ty);
      hint += std::to_string(arg);
    } else {
      throw std::runtime_error("Unrecognized/Unsupported type out of primitive container types");
    }
  }, node.spec_value());
  _contexts.back().push_instruction(std::move(lineB));
  _contexts.back().add_reg_hint(val_id, hint);

  auto res_id = val_id;
  if(node.need_addr()) {
    res_id = store_into_memory(val_id, ty);
  }
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
  /*
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
      auto str_id = _contexts.back().new_reg_id();
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

  auto res_id = reg0_id;
  if(!node.need_addr()) {
    res_id = load_from_memory(reg0_id, ty);
  }

  // record %x1 (the value)
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
  */
}

void IRGenerator::preVisit(ast::CallExpression &node) {
  // if(node.params_opt()) for(auto &param: node.params_opt()->expr_list())
  //   param->set_addr_needed();
}

void IRGenerator::postVisit(ast::CallExpression &node) {
  // %x0 = call ret_t @func(Ty %1, ...)
  // call void @func(Ty %1, ...)
  // if sret:
  // %res_ptr = alloca ret_t
  // call void @func(ret_t* res_ptr, ...)
  // (load res if required)

  auto func_tp = node.expr()->get_type().get<stype::FunctionType>();
  auto ret_type = IRType(func_tp->ret_type());

  CallInst lineC;
  lineC.func_name = func_tp->ident();
  if(func_tp->impl_type_opt()) {
    // static method
    lineC.func_name = mangle_method(func_tp->impl_type_opt(), lineC.func_name);
  }
  auto res_ptr_id = -1;
  if(func_tp->need_sret()) {
    lineC.ret_type = IRType(_type_pool->make_unit());
    res_ptr_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.type = ret_type;
    lineA.dst = res_ptr_id;
    _contexts.back().push_instruction(std::move(lineA));
    _contexts.back().add_reg_hint(res_ptr_id, "call-sret");
    lineC.args.emplace_back(Operand::make_reg(res_ptr_id, wrap_ref(ret_type)));
  } else {
    lineC.ret_type = ret_type;
  }
  if(node.params_opt()) for(int i = 0; i < node.params_opt()->expr_list().size(); ++i) {
    auto &expr = node.params_opt()->expr_list()[i];
    auto node_id = expr->id();
    auto reg_id = _contexts.back().node_reg_map.at(node_id);
    lineC.args.emplace_back(Operand::make_reg(reg_id, IRType(func_tp->params()[i])));
  }
  if(func_tp->ret_type() == _type_pool->make_unit()) {
    _contexts.back().push_instruction(std::move(lineC));
    // ends.
  } else if(!func_tp->need_sret()) {
    auto res_id = _contexts.back().new_reg_id();
    lineC.dst = res_id;
    _contexts.back().push_instruction(std::move(lineC));
    _contexts.back().add_reg_hint(res_id, "call");
    if(node.need_addr()) {
      res_id = store_into_memory(res_id, ret_type);
    }
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
  } else
    {
    _contexts.back().push_instruction(std::move(lineC));
    if(!node.need_addr()) {
      res_ptr_id = load_from_memory(res_ptr_id, ret_type);
    }
    _contexts.back().node_reg_map.emplace(node.id(), res_ptr_id);
  }
}

void IRGenerator::preVisit(ast::MethodCallExpression &node) {
  // if(node.params_opt()) for(auto &param: node.params_opt()->expr_list())
  //   param->set_addr_needed();
}

void IRGenerator::postVisit(ast::MethodCallExpression &node) {
  // just like call expression, but might pass caller itself.
  // func_name is mangled by its associated type's type hash.

  // due to addr_needed property, the register data shall be one layer ref more than caller type (in stype system).
  // if Self* is needed, just pass it. So no unrecorded copy will happen.
  // if Self is needed, we can then load it.

  auto caller = node.expr()->get_type(); // Self/Self*.
  auto &func_name = node.segment()->ident_seg()->ident();
  auto func_tp = find_asso_method(caller, func_name, _type_pool);
  bool is_caller_ref = false;
  // ty: Self.
  auto ty = IRType(caller);
  if(!func_tp) {
    if(!dynamic_cast<ast::IndexExpression*>(node.expr().get())) {
      is_caller_ref = true;
    }
    // Assume caller = &T / &mut T and find methods of T.
    if(auto r = caller.get_if<stype::RefType>()) {
      ty = IRType(r->inner());
      func_tp = find_asso_method(r->inner(), func_name, _type_pool);
    } else {
      throw std::runtime_error("Caller type mismatch or too complicated");
    }
  }

  auto ret_tp = func_tp->ret_type();

  /* fail?
  if(func_tp->impl_type_opt() != _impl_type) {
    throw std::runtime_error("Unexpected impl type");
  }
  */

  auto caller_id = _contexts.back().node_reg_map.at(node.expr()->id()); // Self*

  CallInst lineC;
  // method mangling
  lineC.func_name = mangle_method(func_tp->impl_type_opt(), func_tp->ident());

  auto res_ptr_id = -1;
  if(func_tp->need_sret()) {
    lineC.ret_type = IRType(_type_pool->make_unit());
    res_ptr_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.type = IRType(ret_tp);
    lineA.dst = res_ptr_id;
    _contexts.back().push_instruction(std::move(lineA));
    // _contexts.back().add_reg_hint_from(res_ptr_id, caller_id, "call-sret");
    _contexts.back().add_reg_hint(res_ptr_id, "method-call-sret");
    lineC.args.emplace_back(Operand::make_reg(res_ptr_id, wrap_ref(IRType(ret_tp))));
  } else {
    lineC.ret_type = IRType(ret_tp);
  }

  // pass self.
  if(func_tp->self_type_opt()) {
    auto self_tp = func_tp->self_type_opt();
    // caller can be T (is_caller_ref = false) or T* (is_caller_ref = true).
    // caller requires T, &T or &mut T.
    bool is_self_ref = static_cast<bool>(self_tp.get_if<stype::RefType>());
    if(is_caller_ref && is_self_ref) {
      // caller T**, caller req T*
      // load once.
      caller_id = load_from_memory(caller_id, wrap_ref(ty));
    } else if(is_caller_ref && !is_self_ref) {
      // caller T**, caller req T.
      // load twice.
      caller_id = load_from_memory(caller_id, wrap_ref(ty));
      caller_id = load_from_memory(caller_id, ty);
    } else if(!is_caller_ref && !is_self_ref) {
      // caller T*, caller req T.
      // load once.
      caller_id = load_from_memory(caller_id, ty);
    } else {
      // caller T*, caller req T*.
      // just as needed. do nothing.
    }
    lineC.args.emplace_back(Operand::make_reg(caller_id, IRType(self_tp)));
  }


  // IR reg id order...
  auto res_id = -1;
  if(ret_tp != _type_pool->make_unit()) {
    res_id = _contexts.back().new_reg_id();
    lineC.dst = res_id;
  }

  if(node.params_opt()) for(int i = 0; i < node.params_opt()->expr_list().size(); ++i) {
    auto &expr = node.params_opt()->expr_list()[i];
    auto node_id = expr->id();
    auto reg_id = _contexts.back().node_reg_map.at(node_id);
    lineC.args.emplace_back(Operand::make_reg(reg_id, IRType(func_tp->params()[i])));
  }
  _contexts.back().push_instruction(std::move(lineC));

  if(func_tp->need_sret()) {
    if(!node.need_addr()) {
      res_ptr_id = load_from_memory(res_ptr_id, IRType(ret_tp));
    }
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
  } else if(ret_tp != _type_pool->make_unit()) {
    // _contexts.back().add_reg_hint_from(res_id, caller_id, "call");
    _contexts.back().add_reg_hint(res_id, "method-call");
    if(node.need_addr()) {
      res_id = store_into_memory(res_id, IRType(ret_tp));
    }
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
  }
}

void IRGenerator::preVisit(ast::LetStatement &node) {
  if(node.expr_opt()) node.expr_opt()->set_addr_needed();
  if(_contexts.back().sret_target.has_value()) {
    auto ip = dynamic_cast<ast::IdentifierPattern*>(node.pattern().get());
    if(!ip) { throw std::runtime_error("let pattern too complicated"); }
    if(_contexts.back().sret_target->first == ip->ident()) {
      if(node.expr_opt()) {
        _contexts.back().in_place_node_ptr_map.emplace(node.expr_opt()->id(), _contexts.back().sret_target->second);
      }
    }
  }
}

void IRGenerator::postVisit(ast::LetStatement &node) {
  // binding... whatever.
  auto ip = dynamic_cast<ast::IdentifierPattern*>(node.pattern().get());
  if(!ip) { throw std::runtime_error("let pattern too complicated"); }
  // type tag first; or something like let a: i32 = 1 (deduced to kInt) might happen. Avoiding it.
  auto ty = IRType(node.type_opt() ? node.type_opt()->get_type() : node.expr_opt()->get_type());
  if(!ty) { throw std::runtime_error("No type in let statement"); }
  auto res_id;
  if(node.expr_opt()) {
    // shall already have got a pointer.
    res_id = _contexts.back().node_reg_map.at(node.expr_opt()->id());
    _contexts.back().add_reg_hint(res_id, ip->ident() + ".addr"); // tag override happens
    if(!(_contexts.back().sret_target.has_value() || _contexts.back().sret_target->first == ip->ident())
      && node.expr_opt()->can_summon_lvalue()) {
      // in case this is a lvalue (can't move the pointer directly)
      // give it a copy.
      res_id = load_from_memory(res_id, ty);
      _contexts.back().add_reg_hint(res_id, ip->ident());
      res_id = store_into_memory(res_id, ty);
      _contexts.back().add_reg_hint(res_id, ip->ident() + ".addr");
    }
  } else if(_contexts.back().sret_target.has_value() && _contexts.back().sret_target->first == ip->ident()) {
    res_id = _contexts.back().sret_target->second;
  } else {
    // alloca one place.
    // Warning: Uninitialized variable
    res_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.dst = res_id;
    lineA.type = ty;
    _contexts.back().push_instruction(std::move(lineA));
    _contexts.back().add_reg_hint(res_id, ip->ident() + ".uninitialized-addr");
  }

  if(auto it = _contexts.back().variable_addr_reg_maps.back().find(ip->ident());
    it != _contexts.back().variable_addr_reg_maps.back().end()) {
    // annoying variable shadowing...
    // just remove the previous record.
    // the exact type is flushed here.
    _contexts.back().variable_addr_reg_maps.back().erase(it);
  }
  _contexts.back().variable_addr_reg_maps.back().emplace(ip->ident(), std::pair(res_id, ty));
}

void IRGenerator::postVisit(ast::AssignmentExpression &node) {
  // lhs shall return a pointer Ty*, and rhs shall return a value Ty.
  // Just use one store.
  auto lhs_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  auto rhs_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  auto ty = IRType(node.expr2()->get_type());

  // store Ty %rhs, Ty* %lhs
  StoreInst lineS;
  // might be mut Ty <- Ty, so expr1()->get_type() might be Ty, not Ty*...?
  lineS.ptr = Operand::make_reg(lhs_id, wrap_ref(ty));
  lineS.value = Operand::make_reg(rhs_id, ty);
  _contexts.back().push_instruction(std::move(lineS));

  // still stands for the rhs_id register.
  _contexts.back().node_reg_map.emplace(node.id(), rhs_id);
}

void IRGenerator::preVisit(ast::BorrowExpression &node) {
  node.expr()->set_addr_needed();
}

void IRGenerator::postVisit(ast::BorrowExpression &node) {
  // &val: add a layer of pointer
  // use AllocaInst and StoreInst.
  // %lhs = alloca Ty
  // store Ty %rhs, Ty* %lhs
  // record lhs
  // if need addr: record rhs
  auto rhs_id = _contexts.back().node_reg_map.at(node.expr()->id());
  auto lhs_id = rhs_id;
  if(node.need_addr()) {
    lhs_id = store_into_memory(rhs_id, IRType(node.get_type()));
  }
  _contexts.back().node_reg_map.emplace(node.id(), lhs_id);
}

void IRGenerator::postVisit(ast::DereferenceExpression &node) {
  // *ptr: deprive one layer of pointer.
  // %lhs = load Ty, Ty* %rhs
  // record lhs
  // if need addr: just record rhs
  auto rhs_id = _contexts.back().node_reg_map.at(node.expr()->id());
  auto lhs_id = rhs_id;
  if(!node.need_addr()) {
    lhs_id = load_from_memory(rhs_id, IRType(node.get_type()));
  }
  _contexts.back().node_reg_map.emplace(node.id(), lhs_id);
}

void IRGenerator::postVisit(ast::PathInExpression &node) {
  if(_is_in_const) return; // ignore consteval: should be done in semantic check.
  // if you found it as a function name, ignore it. needless to be handled here.
  // if you found it as a variable name: grab its value.
  // enum variable... sorry I can't do it.
  if(node.get_type().get_if<stype::FunctionType>()) {
    // function type already filled. CallExpression know what to call.
    return;
  }
  if(node.segments().size() != 1) {
    throw std::runtime_error("PathInExpression too complicated");
  }
  auto ident = node.segments().back()->ident_seg()->ident();

  if(ident == "self") {
    // "self" shall be already registered into node_reg_map. just find it like normal variables.
    // nothing extra to do here.
  } else {
    auto info = find_symbol(ident);
    if(!info) {
      throw std::runtime_error("Info of variable not found: " + ident);
    }
    if(info->kind == ast::SymbolKind::kConstant) {
      // value must be inlined. we only support integers here.
      auto value_opt = info->cval->get_if<sconst::ConstPrime>()->get_integer();
      if(!value_opt) {
        throw std::runtime_error("Constant not a integer");
      }
      auto value = value_opt.value();
      // record a register that contains this value.
      // %res = add Ty 0, value
      auto ty = IRType(info->cval->type());
      auto res_id = _contexts.back().new_reg_id();
      BinaryOpInst lineB;
      lineB.lhs = Operand::make_imm(0, ty);
      lineB.rhs = Operand::make_imm(value, ty);
      lineB.type = ty;
      lineB.dst = res_id;
      lineB.op = "add";
      _contexts.back().push_instruction(std::move(lineB));
      _contexts.back().add_reg_hint(res_id, "const-" + std::to_string(value));
      // need addr: store.
      if(node.need_addr()) {
        res_id = store_into_memory(res_id, ty);
      }
      _contexts.back().node_reg_map.emplace(node.id(), res_id);
      return;
    }
    if(info->kind != ast::SymbolKind::kVariable) return;
    // Do not rely on info->type: it only records the last type affected by variable shadowing.
  }

  auto [val_ptr_id, ty] = _contexts.back().find_variable(ident);
  _contexts.back().add_reg_hint(val_ptr_id, ident + ".addr");

  auto here_id = val_ptr_id;
  // lside: record Ty*
  // rside: load, and record Ty
  if(!node.need_addr()) {
    here_id = load_from_memory(val_ptr_id, ty);
    _contexts.back().add_reg_hint(here_id, ident);
  }

  _contexts.back().node_reg_map.emplace(node.id(), here_id);
}

void IRGenerator::preVisit(ast::IndexExpression &node) {
  node.expr_obj()->set_addr_needed();
}

void IRGenerator::postVisit(ast::IndexExpression &node) {
  if(_is_in_const) return;
  // input - obj: (&(mut)) [T; N], index: usize.
  // output - obj[index]: T.
  // getelementptr is needed.

  auto obj_id = _contexts.back().node_reg_map.at(node.expr_obj()->id());
  auto index_id = _contexts.back().node_reg_map.at(node.expr_index()->id());

  auto ptr_id = -1; // ArrTy*
  auto arr_ty = IRType(node.expr_obj()->get_type());
  if(node.expr_obj()->need_addr()) {
    // I know I modified it... sigh.
    arr_ty = wrap_ref(arr_ty);
  }
  if(!arr_ty.type().get_if<stype::RefType>()) {
    // if given [T; N], then it must be on rside. put it in memory.
    // store the array into the memory.
    // %ptr = alloca ArrTy
    // store ArrTy %obj, ArrTy* %ptr
    ptr_id = store_into_memory(obj_id, arr_ty);
  } else {
    // if given [T; N]* / **, then the address is needed. deref it to [T; N]*.
    ptr_id = obj_id;
    arr_ty = IRType(arr_ty.type().get_if<stype::RefType>()->inner());
    while(true) {
      auto r = arr_ty.type().get_if<stype::RefType>();
      if(!r) break;
      ptr_id = load_from_memory(ptr_id, arr_ty);
      arr_ty = IRType(r->inner());
    }
  }

  // ptr is ArrTy*, or [T; N]* now.
  auto dst_id = _contexts.back().new_reg_id();
  // (T*) %dst = getelementptr [N x T], [N x T]* %ptr, i32 0, index_t %idx
  GEPInst lineG;
  lineG.dst = dst_id;
  lineG.base_type = arr_ty;
  lineG.ptr = Operand::make_reg(ptr_id, wrap_ref(arr_ty));
  auto i32_type = IRType(_type_pool->make_i32());
  lineG.indices.emplace_back(Operand::make_imm(0, i32_type));
  lineG.indices.emplace_back(Operand::make_reg(index_id, IRType(node.expr_index()->get_type())));
  _contexts.back().push_instruction(std::move(lineG));

  auto val_id = dst_id;
  // if it's on lside, we need it to remain &T (Ty*);
  // if it's on rside, we need to load its value (T, Ty).
  if(!node.need_addr()) {
    val_id = load_from_memory(dst_id, IRType(node.get_type()));
  }
  // record %val
  _contexts.back().node_reg_map.emplace(node.id(), val_id);
}

void IRGenerator::postVisit(ast::TypeCastExpression &node) {
  if(_is_in_const) return;
  // %c = bitcast/zext/sext/trunc Ty1* %p to Ty2*
  auto ty1 = IRType(node.expr()->get_type());
  auto ty2 = IRType(node.type_no_bounds()->get_type());
  auto src_id = _contexts.back().node_reg_map.at(node.expr()->id());

  // if the two types are the same in LLVM (like, i32 and usize are both "i32")
  // skip this cast instruction.
  auto dst_id = src_id;
  if(ty1.to_str() != ty2.to_str()) { // use LLVM str to compare...
    dst_id = _contexts.back().new_reg_id();
    CastInst lineC;
    lineC.dst = dst_id;
    lineC.dst_type = ty2;
    lineC.src = Operand::make_reg(src_id, ty1);
    _contexts.back().push_instruction(std::move(lineC));
  }

  if(node.need_addr()) {
    dst_id = store_into_memory(dst_id, IRType(node.get_type()));
  }

  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::postVisit(ast::NegationExpression &node) {
  if(_is_in_const) return;
  // unary operation: LogicalNot and ArithmeticInverse
  auto val_id = _contexts.back().node_reg_map.at(node.expr()->id());
  auto dst_id = _contexts.back().new_reg_id();
  auto ty = IRType(node.expr()->get_type());
  if(node.oper() == ast::Operator::kLogicalNot) {
    // !val -> val == 0
    // %dst = icmp eq Ty %val, 0
    BinaryOpInst lineB;
    lineB.lhs = Operand::make_reg(val_id, ty);
    lineB.rhs = Operand::make_imm(0, ty);
    lineB.type = ty;
    lineB.dst = dst_id;
    lineB.op = "icmp eq";
    _contexts.back().push_instruction(std::move(lineB));

    // (for that !i32's sake, change the value type to Ty.)
    // %res = bitcast i1 %dst to Ty
    if(ty.to_str() != "i1") {
      auto res_id = _contexts.back().new_reg_id();
      CastInst lineC;
      lineC.dst = res_id;
      lineC.src = Operand::make_reg(dst_id, IRType(_type_pool->make_bool()));
      lineC.dst_type = ty;
      _contexts.back().push_instruction(std::move(lineC));

      dst_id = res_id;
    }
  } else if(node.oper() == ast::Operator::kSub) {
    // -val -> 0 - val
    // %dst = sub Ty 0, %val
    BinaryOpInst lineB;
    lineB.lhs = Operand::make_imm(0, ty);
    lineB.rhs = Operand::make_reg(val_id, ty);
    lineB.type = ty;
    lineB.dst = dst_id;
    lineB.op = "sub";
    _contexts.back().push_instruction(std::move(lineB));
  }
  if(node.need_addr()) {
    dst_id = store_into_memory(dst_id, ty);
  }
  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::visit(ast::ArithmeticOrLogicalExpression &node) {
  RecursiveVisitor::preVisit(node);
  if(_is_in_const) return;
  // binary operation:
  // %dst = icmp op Ty %val1, %val2
  auto ty = IRType(node.get_type());

  BinaryOpInst lineB;
  lineB.type = ty;
  if(node.expr1()->has_constant()) {
    auto tp1 = node.expr1()->cval()->type();
    if(auto p1 = tp1.get_if<stype::PrimeType>(); !p1 || !p1->is_number()) {
      throw std::runtime_error("Literal operand 1 not number");
    }
    auto val = node.expr1()->cval()->get_if<sconst::ConstPrime>()->get_integer().value();
    lineB.lhs = Operand::make_imm(val, ty);
  } else {
    node.expr1()->accept(*this);
    auto val1_id = _contexts.back().node_reg_map.at(node.expr1()->id());
    lineB.lhs = Operand::make_reg(val1_id, ty);
  }
  if(node.expr2()->has_constant()) {
    auto tp2 = node.expr2()->cval()->type();
    if(auto p2 = tp2.get_if<stype::PrimeType>(); !p2 || !p2->is_number()) {
      throw std::runtime_error("Literal operand 1 not number");
    }
    auto val = node.expr2()->cval()->get_if<sconst::ConstPrime>()->get_integer().value();
    lineB.rhs = Operand::make_imm(val, ty);
  } else {
    node.expr2()->accept(*this);
    auto val2_id = _contexts.back().node_reg_map.at(node.expr2()->id());
    lineB.rhs = Operand::make_reg(val2_id, ty);
  }
  auto dst_id = _contexts.back().new_reg_id();
  lineB.dst = dst_id;

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


  if(node.need_addr()) {
    dst_id = store_into_memory(dst_id, ty);
  }

  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
  RecursiveVisitor::postVisit(node);
}

void IRGenerator::postVisit(ast::ComparisonExpression &node) {
  if(_is_in_const) return;
  // binary operation:
  // %dst = op Ty %val1, %val2
  auto val1_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  auto val2_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  auto dst_id = _contexts.back().new_reg_id();

  auto ty = IRType(node.expr1()->get_type()); // operand type, not result type
  BinaryOpInst lineB;
  lineB.lhs = Operand::make_reg(val1_id, ty);
  lineB.rhs = Operand::make_reg(val2_id, ty);
  lineB.type = ty;
  lineB.dst = dst_id;

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
    throw std::runtime_error("Invalid operator type in comparison expression");
  }
  _contexts.back().push_instruction(std::move(lineB));

  if(node.need_addr()) {
    dst_id = store_into_memory(dst_id, IRType(_type_pool->make_bool()));
  }

  _contexts.back().node_reg_map.emplace(node.id(), dst_id);
}

void IRGenerator::visit(ast::CompoundAssignmentExpression &node) {
  node.expr1()->accept(*this);
  auto ptr1_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  // a x= b -> _ = a x b, a = _.
  // a: Ty*, b: Ty.
  auto ty = IRType(node.expr2()->get_type());
  auto ty_ref = wrap_ref(ty);

  auto val1_id = _contexts.back().new_reg_id();
  // %val1 = load Ty, Ty* %ptr1
  LoadInst lineL;
  lineL.dst = val1_id;
  lineL.load_type = ty;
  lineL.ptr = Operand::make_reg(ptr1_id, ty_ref);
  _contexts.back().push_instruction(std::move(lineL));

  // %res = icmp op Ty %val1, %val2

  BinaryOpInst lineB;
  lineB.type = ty;
  lineB.lhs = Operand::make_reg(val1_id, ty);
  if(node.expr2()->has_constant()) {
    auto tp2 = node.expr2()->cval()->type();
    if(auto p2 = tp2.get_if<stype::PrimeType>(); !p2 || !p2->is_number()) {
      throw std::runtime_error("Literal operand 1 not number");
    }
    auto val = node.expr2()->cval()->get_if<sconst::ConstPrime>()->get_integer().value();
    lineB.rhs = Operand::make_imm(val, ty);
  } else {
    node.expr2()->accept(*this);
    auto val2_id = _contexts.back().node_reg_map.at(node.expr2()->id());
    lineB.rhs = Operand::make_reg(val2_id, ty);
  }
  auto res_id = _contexts.back().new_reg_id();
  lineB.dst = res_id;

  auto ty1 = node.expr1()->get_type().get_if<stype::RefType>();
  auto prime1 = ty1 ? ty1->inner().get_if<stype::PrimeType>() : node.expr1()->get_type().get_if<stype::PrimeType>();
  auto prime2 = node.expr2()->get_type().get_if<stype::PrimeType>();

  if(!prime1 || !prime2) {
    throw std::runtime_error("Operands too complicated in Arithmetic/Logical Expression");
  }
  bool is_unsigned = prime1->is_unsigned_int();
  switch(node.oper()) {
  case ast::Operator::kShlAssign: {
    lineB.op = "shl";
  } break;
  case ast::Operator::kShrAssign: {
    lineB.op = is_unsigned ? "lshr" : "ashr";
  } break;
  case ast::Operator::kAddAssign: {
    lineB.op = "add";
  } break;
  case ast::Operator::kSubAssign: {
    lineB.op = "sub";
  } break;
  case ast::Operator::kMulAssign: {
    lineB.op = "mul";
  } break;
  case ast::Operator::kDivAssign: {
    lineB.op = is_unsigned ? "udiv" : "sdiv";
  } break;
  case ast::Operator::kModAssign: {
    lineB.op = is_unsigned ? "urem" : "srem";
  } break;
  case ast::Operator::kBitwiseAndAssign: {
    lineB.op = "and";
  } break;
  case ast::Operator::kBitwiseOrAssign: {
    lineB.op = "or";
  } break;
  case ast::Operator::kBitwiseXorAssign: {
    lineB.op = "xor";
  } break;
  default:
    throw std::runtime_error("Invalid operator type in compound assignment expression");
  }
  _contexts.back().push_instruction(std::move(lineB));

  // store Ty %res, Ty* %ptr1
  StoreInst lineS;
  lineS.ptr = Operand::make_reg(ptr1_id, ty_ref);
  lineS.value = Operand::make_reg(res_id, ty);
  _contexts.back().push_instruction(std::move(lineS));

  // record nothing. It should return a unit (void)
  // _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::preVisit(ast::FieldExpression &node) {
  node.expr()->set_addr_needed();
}

void IRGenerator::postVisit(ast::FieldExpression &node) {
  auto obj_id = _contexts.back().node_reg_map.at(node.expr()->id());
  auto ftp = IRType(node.expr()->get_type());
  if(node.expr()->need_addr()) {
    // I know I modified it... sigh.
    ftp = wrap_ref(ftp);
  }
  auto struct_ptr = ftp.type().get_if<stype::StructType>();
  auto ty = IRType(ftp);
  if(!struct_ptr) {
    auto p = ftp.type();
    while(p.get_if<stype::RefType>()) {
      p = p.get_if<stype::RefType>()->inner();
    }
    ty = IRType(p);
    struct_ptr = p.get_if<stype::StructType>();
    if(!struct_ptr) {
      throw std::runtime_error("Not a struct");
    }
  }
  auto [diff_idx, field_ty] = struct_ptr->field_orders().at(node.ident());
  auto ty_ref = wrap_ref(ty);

  auto ptr_id = obj_id;
  if(!ftp.type().get_if<stype::RefType>()) {
    // value (meaning that it's not on lside; put it into memory)
    // %ptr = alloca STy
    // store STy %obj, STy* %ptr
    ptr_id = store_into_memory(obj_id, ty);
  } else {
    // auto deref: deref to STy*
    ptr_id = obj_id;
    while(true) {
      auto r = ftp.type().get_if<stype::RefType>();
      if(r && !r->inner().get_if<stype::RefType>()) break;
      ftp = IRType(r->inner());
      ptr_id = _contexts.back().new_reg_id();
      LoadInst lineL;
      lineL.load_type = ftp;
      lineL.ptr = Operand::make_reg(obj_id, wrap_ref(ftp));
      lineL.dst = ptr_id;
      _contexts.back().push_instruction(std::move(lineL));
      obj_id = ptr_id;
    }
  }

  auto rptr_id = _contexts.back().new_reg_id();
  // %rptr = getelementptr STy, STy* %ptr, i32 0, i32(usize) diff_idx
  GEPInst lineG;
  lineG.dst = rptr_id;
  lineG.base_type = IRType(ty);
  lineG.ptr = Operand::make_reg(ptr_id, ty_ref);
  auto i32_tp = IRType(_type_pool->make_i32());
  lineG.indices.emplace_back(Operand::make_imm(0, i32_tp));
  lineG.indices.emplace_back(Operand::make_imm(diff_idx, i32_tp));
  _contexts.back().push_instruction(std::move(lineG));

  // if on lside, record the pointer rptr.
  // if on rside, record a value res.
  auto res_id = rptr_id;
  if(!node.need_addr()) {
    res_id = load_from_memory(rptr_id, IRType(field_ty));
  }
  // record %res
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::postVisit(ast::GroupedExpression &node) {
  if(_is_in_const) return;
  auto res_id = _contexts.back().node_reg_map.at(node.expr()->id());

  if(node.need_addr()) {
    auto ptr_id = _contexts.back().new_reg_id();
    auto ty = IRType(node.get_type());
    AllocaInst lineA;
    lineA.dst = ptr_id;
    lineA.type = ty;
    _contexts.back().push_instruction(std::move(lineA));
    StoreInst lineS;
    lineS.ptr = Operand::make_reg(ptr_id, wrap_ref(ty));
    lineS.value = Operand::make_reg(res_id, ty);
    _contexts.back().push_instruction(std::move(lineS));
    res_id = ptr_id;
  }

  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

void IRGenerator::visit(ast::StructExpression &node) {
  RecursiveVisitor::preVisit(node);
  auto struct_ty = node.get_type();
  auto stp = struct_ty.get_if<stype::StructType>();

  auto struct_ptr_id = -1;
  if(auto it = _contexts.back().in_place_node_ptr_map.find(node.id());
    it != _contexts.back().in_place_node_ptr_map.end()) {
    struct_ptr_id = it->second;
  } else {
    struct_ptr_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.type = IRType(struct_ty);
    lineA.dst = struct_ptr_id;
    _contexts.back().push_instruction(std::move(lineA));
  }

  // order of field in the struct, field_ty, its node id, the field expr node pointer
  struct Record {
    int order;
    IRType type;
    int node_id;
    ast::NamedStructExprField* field_ptr;
  };
  std::vector<Record> records;
  if(node.fields_opt()) for(auto &sefp: node.fields_opt()->fields()) {
    if(!sefp->is_named()) {
      throw std::runtime_error("Unsupported index struct field");
    }
    auto ptr = dynamic_cast<ast::NamedStructExprField*>(sefp.get());
    auto ident = ptr->ident();
    auto [order, ty] = stp->field_orders().at(ident);
    auto node_id = ptr->expr()->id();
    records.emplace_back(order, IRType(ty), node_id, ptr);
  }
  std::sort(records.begin(), records.end(),
    [](const Record &A, const Record &B) {
      return A.order < B.order;
    });

  auto i32_tp = IRType(_type_pool->make_i32());

  for(int i = 0; i < stp->fields().size(); ++i) {
    auto elem_ptr_id = _contexts.back().new_reg_id();
    // %elem_ptr = getelementptr St, St* %struct_ptr, i32 0, i32 i
    GEPInst lineG;
    lineG.dst = elem_ptr_id;
    lineG.ptr = Operand::make_reg(struct_ptr_id, wrap_ref(IRType(struct_ty)));
    lineG.base_type = IRType(struct_ty);
    lineG.indices.emplace_back(Operand::make_imm(0, i32_tp));
    lineG.indices.emplace_back(Operand::make_imm(i, i32_tp));
    _contexts.back().push_instruction(std::move(lineG));

    if(auto expr = dynamic_cast<ast::ArrayExpression*>(records[i].field_ptr->expr().get())) {
      _contexts.back().in_place_node_ptr_map.emplace(expr->id(), elem_ptr_id);
      records[i].field_ptr->accept(*this);
      _contexts.back().in_place_node_ptr_map.erase(expr->id());
      // shall already be constructed.
    } else {
      auto &record = records[i];
      record.field_ptr->accept(*this);
      // store Ty %val, Ty* %elem_ptr
      auto value_id = _contexts.back().node_reg_map.at(record.node_id);
      StoreInst lineS;
      lineS.ptr = Operand::make_reg(elem_ptr_id, wrap_ref(record.type));
      lineS.value = Operand::make_reg(value_id, record.type);
      _contexts.back().push_instruction(std::move(lineS));
    }
  }

  auto res_id = struct_ptr_id;
  if(!node.need_addr()) {
    res_id = load_from_memory(struct_ptr_id, IRType(struct_ty));
  }

  _contexts.back().node_reg_map.emplace(node.id(), res_id);
  RecursiveVisitor::postVisit(node);
}

void IRGenerator::visit(ast::ArrayExpression &node) {
  RecursiveVisitor::preVisit(node);
  auto arr_tp = node.get_type();
  auto length = arr_tp.get_if<stype::ArrayType>()->length();
  auto tp = arr_tp.get_if<stype::ArrayType>()->inner();
  auto i32_tp = IRType(_type_pool->make_i32());

  reg_id_t res_id = -1;
  reg_id_t arr_ptr_id = -1;
  bool is_in_place_construction = false;
  if(auto it = _contexts.back().in_place_node_ptr_map.find(node.id());
    it != _contexts.back().in_place_node_ptr_map.end()) {
    arr_ptr_id = it->second;
    is_in_place_construction = true;
    } else {
      arr_ptr_id = _contexts.back().new_reg_id();
      AllocaInst lineA;
      lineA.type = IRType(arr_tp);
      lineA.dst = arr_ptr_id;
      _contexts.back().push_instruction(std::move(lineA));
    }
  if(!node.elements_opt()) {
    throw std::runtime_error("Empty array?");
  } else if(node.elements_opt()->is_explicit()) {
    auto eptr = dynamic_cast<ast::ExplicitArrayElements*>(node.elements_opt().get());
    //   ([T; N]*) %arr_ptr = alloca [T; N] / use the given arr_ptr
    // (repeat: i = 0 to N-1)
    //   (T*) %elem_ptr = getelementptr [T; N], [T; N]* %arr_ptr, i32 0, i32 i
    //   store T %val_i, T* %elem_ptr
    //   #or directly construct on %elem_ptr
    //
    //   (%arr_val = load [T; N], [T; N]* %arr_ptr)

    for(int i = 0; i < length; ++i) {
      auto elem_ptr_id = _contexts.back().new_reg_id();
      GEPInst lineG;
      lineG.ptr = Operand::make_reg(arr_ptr_id, wrap_ref(IRType(arr_tp)));
      lineG.base_type = IRType(arr_tp);
      lineG.dst = elem_ptr_id;
      lineG.indices.emplace_back(Operand::make_imm(0, i32_tp));
      lineG.indices.emplace_back(Operand::make_imm(i, i32_tp));
      _contexts.back().push_instruction(std::move(lineG));

      auto expr = eptr->expr_list()[i].get();
      if(dynamic_cast<ast::ArrayExpression*>(expr)) {
        _contexts.back().in_place_node_ptr_map.emplace(expr->id(), elem_ptr_id);
        expr->accept(*this);
        _contexts.back().in_place_node_ptr_map.erase(expr->id());
        // construction shall be completed.
      } else {
        expr->accept(*this);
        // manually construct from value
        auto value_id = _contexts.back().node_reg_map.at(expr->id());
        StoreInst lineS;
        lineS.ptr = Operand::make_reg(elem_ptr_id, wrap_ref(IRType(tp)));
        lineS.value = Operand::make_reg(value_id, IRType(tp));
        _contexts.back().push_instruction(std::move(lineS));
      }
    }
  } else {
    // [f(); N] will call f() only once and replicate it N times.
    // just copy it.
    auto rptr = dynamic_cast<ast::RepeatedArrayElements*>(node.elements_opt().get());
    rptr->val_expr()->accept(*this);
    rptr->len_expr()->accept(*this); // needed?
    //   ([T; N]*) %arr_ptr = alloca [T; N] / use the given arr_ptr
    //   (T*) %ptr = getelementptr [T; N], [T; N]* %arr_ptr, i32 0, i32 0
    //   %cnt_ptr = alloca i32
    //   %store i32 0, i32* %cnt_ptr
    //   br label %new.arr.cond:
    // new.arr.cond:
    //   %cnt = load i32, i32* %cnt_ptr
    //   %cmp = icmp slt i32 %cnt, N
    //   br i1 %cmp, label %new.arr.body, label %new.arr.exit
    // new.arr.body:
    //   (T*) %elem_ptr = getelementptr T, T* %ptr, i32 %cnt
    //   store T %val, T* %elem_ptr
    //   %nxt_cnt = add i32 %cnt, 1
    //   store i32 %nxt_cnt, i32* %cnt_ptr
    //   br label %new.arr.cond
    // new.arr.exit:
    //   (%arr_val = load [T; N], [T; N]* %arr_ptr)

    auto hint_tag_id = _contexts.back().new_hint_tag_id();
    Label cond_label(LabelHint::kArrayCond, hint_tag_id);
    Label body_label(LabelHint::kArrayBody, hint_tag_id);
    Label exit_label(LabelHint::kArrayExit, hint_tag_id);

    if(auto it = _contexts.back().in_place_node_ptr_map.find(node.id());
      it != _contexts.back().in_place_node_ptr_map.end()) {
      arr_ptr_id = it->second;
      } else {
        arr_ptr_id = _contexts.back().new_reg_id();
        AllocaInst lineA;
        lineA.type = IRType(arr_tp);
        lineA.dst = arr_ptr_id;
        _contexts.back().push_instruction(std::move(lineA));
      }

    auto ptr_id = _contexts.back().new_reg_id();
    GEPInst lineG;
    lineG.ptr = Operand::make_reg(arr_ptr_id, wrap_ref(IRType(arr_tp)));
    lineG.base_type = IRType(arr_tp);
    lineG.dst = ptr_id;
    lineG.indices.emplace_back(Operand::make_imm(0, i32_tp));
    lineG.indices.emplace_back(Operand::make_imm(0, i32_tp));
    _contexts.back().push_instruction(std::move(lineG));
    lineG.indices.clear();

    auto cnt_ptr_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.type = i32_tp;
    lineA.dst = cnt_ptr_id;
    _contexts.back().push_instruction(std::move(lineA));

    StoreInst lineS;
    lineS.ptr = Operand::make_reg(cnt_ptr_id, wrap_ref(i32_tp));
    lineS.value = Operand::make_imm(0, i32_tp);
    _contexts.back().push_instruction(std::move(lineS));

    BranchInst lineB;
    lineB.label = cond_label;
    _contexts.back().push_instruction(std::move(lineB));

    _contexts.back().init_and_start_new_block(cond_label);

    auto cnt_id = _contexts.back().new_reg_id();
    LoadInst lineL;
    lineL.ptr = Operand::make_reg(cnt_ptr_id, wrap_ref(i32_tp));
    lineL.dst = cnt_id;
    lineL.load_type = i32_tp;
    _contexts.back().push_instruction(std::move(lineL));

    auto cmp_id = _contexts.back().new_reg_id();
    BinaryOpInst lineBO;
    lineBO.op = "icmp slt";
    lineBO.type = i32_tp;
    lineBO.dst = cmp_id;
    lineBO.lhs = Operand::make_reg(cnt_id, i32_tp);
    lineBO.rhs = Operand::make_imm(length, i32_tp);
    _contexts.back().push_instruction(std::move(lineBO));

    CondBranchInst lineC;
    lineC.cond = cmp_id;
    lineC.true_label = body_label;
    lineC.false_label = exit_label;
    _contexts.back().push_instruction(std::move(lineC));

    _contexts.back().init_and_start_new_block(body_label);

    auto elem_ptr_id = _contexts.back().new_reg_id();
    lineG.base_type = IRType(tp);
    lineG.ptr = Operand::make_reg(ptr_id, wrap_ref(IRType(tp)));
    lineG.dst = elem_ptr_id;
    lineG.indices.emplace_back(Operand::make_reg(cnt_id, i32_tp));
    _contexts.back().push_instruction(std::move(lineG));
    lineG.indices.clear();

    auto expr = rptr->val_expr().get();
    if(dynamic_cast<ast::ArrayExpression*>(expr)) {
      _contexts.back().in_place_node_ptr_map.emplace(expr->id(), elem_ptr_id);
      expr->accept(*this);
      _contexts.back().in_place_node_ptr_map.erase(expr->id());
      // construction shall be completed.
    } else {
      expr->accept(*this);
      // manually construct from value
      auto value_id = _contexts.back().node_reg_map.at(expr->id());
      lineS.ptr = Operand::make_reg(elem_ptr_id, wrap_ref(IRType(tp)));
      lineS.value = Operand::make_reg(value_id, IRType(tp));
      _contexts.back().push_instruction(std::move(lineS));
    }

    auto nxt_cnt_id = _contexts.back().new_reg_id();
    lineBO.op = "add";
    lineBO.type = i32_tp;
    lineBO.lhs = Operand::make_reg(cnt_id, i32_tp);
    lineBO.rhs = Operand::make_imm(1, i32_tp);
    lineBO.dst = nxt_cnt_id;
    _contexts.back().push_instruction(std::move(lineBO));

    lineS.ptr = Operand::make_reg(cnt_ptr_id, wrap_ref(i32_tp));
    lineS.value = Operand::make_reg(nxt_cnt_id, i32_tp);
    _contexts.back().push_instruction(std::move(lineS));

    lineB.label = cond_label;
    _contexts.back().push_instruction(std::move(lineB));

    _contexts.back().init_and_start_new_block(exit_label);
  }

  if(is_in_place_construction) {
    // no one needs the result pointer.
    return;
  }

  res_id = arr_ptr_id;
  if(!node.need_addr()) {
    res_id = load_from_memory(arr_ptr_id, IRType(arr_tp));
  }

  _contexts.back().node_reg_map.emplace(node.id(), res_id);
  RecursiveVisitor::postVisit(node);
}

void IRGenerator::preVisit(ast::BlockExpression &node) {
  ScopedVisitor::preVisit(node);
  _contexts.back().variable_addr_reg_maps.emplace_back();
}

void IRGenerator::postVisit(ast::BlockExpression &node) {
  // if it has no value, do not register anything into node-reg-map.
  if(node.stmts_opt() && node.stmts_opt()->expr_opt()) {
    auto node_id = node.stmts_opt()->expr_opt()->id();
    if(auto it = _contexts.back().node_reg_map.find(node_id);
      it != _contexts.back().node_reg_map.end()) { // might be an if expr...

      auto res_id = it->second;

      if(node.need_addr()) {
        auto ptr_id = _contexts.back().new_reg_id();
        auto ty = IRType(node.get_type());
        AllocaInst lineA;
        lineA.dst = ptr_id;
        lineA.type = ty;
        _contexts.back().push_instruction(std::move(lineA));
        StoreInst lineS;
        lineS.ptr = Operand::make_reg(ptr_id, wrap_ref(ty));
        lineS.value = Operand::make_reg(res_id, ty);
        _contexts.back().push_instruction(std::move(lineS));
        res_id = ptr_id;
      }

      _contexts.back().node_reg_map.emplace(node.id(), res_id);
    }
  }

  // eliminate symbols added in this scope.
  _contexts.back().variable_addr_reg_maps.pop_back();
  ScopedVisitor::postVisit(node);
}

void IRGenerator::postVisit(ast::Conditions &node) {
  auto res_id = _contexts.back().node_reg_map.at(node.expr()->id());
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
  // will never need addr.
}

void IRGenerator::visit(ast::IfExpression &node) {
  // if (cond) { block } else { block (normal / non exist / another if) }
  // label: if.then, if.else, if.exit

  //   #cond...
  //   br i1 %cond_res, label %if.then, label %if.else
  // if.then:
  //   #body
  //   %then_res = ...
  //   (currently in label %from_then)
  //   br label %if.exit
  // if.else:
  // (if there is else)
  //   #else content
  //   %else_res = ...
  //   (currently in label %from_else)
  //   br label $if.exit
  // (\if there is else)
  // if.exit:
  //   %if_res = %then_res / %else_res
  // (if there is else)
  //   %if_res = phi Ty [%then_res, %from_then], [%else_res, %from_else]
  //   (go on)

  auto hint_tag_id = _contexts.back().new_hint_tag_id();
  Label then_label(LabelHint::kIfThen, hint_tag_id);
  Label else_label(LabelHint::kIfElse, hint_tag_id);
  Label exit_label(LabelHint::kIfExit, hint_tag_id);

  node.cond()->accept(*this);

  auto cond_id = _contexts.back().node_reg_map.at(node.cond()->id());
  CondBranchInst lineC;
  lineC.cond = cond_id;
  lineC.true_label = then_label;
  lineC.false_label = else_label;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().init_and_start_new_block(then_label);

  node.block_expr()->accept(*this);

  // then res
  reg_id_t then_id = -1;
  if(auto it = _contexts.back().node_reg_map.find(node.block_expr()->id());
    it != _contexts.back().node_reg_map.end()) {
    then_id = it->second;
  }

  BranchInst lineB;
  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  Label then_from_label = _contexts.back().basic_block_packs.back().label;

  _contexts.back().init_and_start_new_block(else_label);

  reg_id_t else_id = -1;

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

  Label else_from_label = _contexts.back().basic_block_packs.back().label;

  _contexts.back().init_and_start_new_block(exit_label);

  if(then_id == -1 && else_id == -1) {
    // No need to do anything.
    // nothing to record.
    return;
  }
  auto exit_id = _contexts.back().new_reg_id();
  if(then_id != -1 && else_id != -1) {
    auto ty = IRType(node.get_type());
    PhiInst lineP;
    lineP.dst = exit_id;
    lineP.type = ty;
    lineP.incoming.emplace_back(Operand::make_reg(then_id, ty), then_from_label);
    lineP.incoming.emplace_back(Operand::make_reg(else_id, ty), else_from_label);
    _contexts.back().push_instruction(std::move(lineP));
  } else if(then_id != -1) {
    exit_id = then_id;
  } else {
    exit_id = else_id;
  }

  if(node.need_addr()) {
    exit_id = store_into_memory(exit_id, IRType(node.get_type()));
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
  auto hint_tag_id = _contexts.back().new_hint_tag_id();
  Label cond_label(LabelHint::kWhileCond, hint_tag_id);
  Label body_label(LabelHint::kWhileBody, hint_tag_id);
  Label exit_label(LabelHint::kWhileExit, hint_tag_id);

  BranchInst lineB;
  lineB.label = cond_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().init_and_start_new_block(cond_label);

  node.cond()->accept(*this);
  auto cond_id = _contexts.back().node_reg_map.at(node.cond()->id());

  _contexts.back().loop_contexts.emplace_back(cond_label, exit_label, -1);

  CondBranchInst lineC;
  lineC.cond = cond_id;
  lineC.true_label = body_label;
  lineC.false_label = exit_label;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().init_and_start_new_block(body_label);

  node.block_expr()->accept(*this);

  lineB.label = cond_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().init_and_start_new_block(exit_label);

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

  auto hint_tag_id = _contexts.back().new_hint_tag_id();
  Label body_label(LabelHint::kLoopBody, hint_tag_id);
  Label exit_label(LabelHint::kLoopExit, hint_tag_id);

  reg_id_t res_ptr_id = -1;
  if(not_void) {
    res_ptr_id = _contexts.back().new_reg_id();
    AllocaInst lineA;
    lineA.dst = res_ptr_id;
    lineA.type = ty;
    _contexts.back().push_instruction(std::move(lineA));
  }
  BranchInst lineB;
  lineB.label = body_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().init_and_start_new_block(body_label);

  _contexts.back().loop_contexts.emplace_back(body_label, exit_label, res_ptr_id);

  node.block_expr()->accept(*this);

  lineB.label = body_label;
  _contexts.back().push_instruction(std::move(lineB));

  _contexts.back().init_and_start_new_block(exit_label);

  if(not_void) {
    auto res_id = res_ptr_id;
    if(!node.need_addr()) {
      res_id = load_from_memory(res_ptr_id, ty);
    }
    // record res
    _contexts.back().node_reg_map.emplace(node.id(), res_id);
  }

  _contexts.back().loop_contexts.pop_back();
}

void IRGenerator::postVisit(ast::BreakExpression &node) {
  if(node.expr_opt()) {
    // break value;
    auto value_id = _contexts.back().node_reg_map.at(node.expr_opt()->id());
    auto ty = IRType(node.expr_opt()->get_type());
    // must be InfiniteLoop, and res_ptr_id is not -1.
    auto res_ptr_id = _contexts.back().loop_contexts.back().res_ptr_id;
    // store Ty &value, Ty* %res_ptr
    StoreInst lineS;
    lineS.ptr = Operand::make_reg(res_ptr_id, wrap_ref(ty));
    lineS.value = Operand::make_reg(value_id, ty);
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
    lineR.ret_val = std::nullopt;
  } else {
    auto reg_id = _contexts.back().node_reg_map.at(node.expr_opt()->id());
    IRType ret_type(node.expr_opt()->get_type());
    lineR.ret_val = Operand::make_reg(reg_id, ret_type);
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
  //   (now at label %then_from)
  //   br label %laz.exit
  // %laz.else:
  //   (now at label %else_from)
  //   br label %laz.exit
  // %laz.exit:
  //   %res = phi i1 [%rhs, %then_from], [0, %else_from]

  // res = lhs || rhs ->
  // res = if (lhs) { true } else { rhs }

  //   ...(lhs ready, rhs not ready)
  //   br i1 %lhs, label %laz.then, label %lazy_bool.else
  // %laz.then:
  //   (now at label %then_from)
  //   br label %laz.exit
  // %laz.else:
  //   (calc rhs here)
  //   (now at label %else_from)
  //   br label %laz.exit
  // %laz.exit:
  //   %res = phi i1 [1, %then_from], [%rhs, %else_from]

  node.expr1()->accept(*this);
  auto lhs_id = _contexts.back().node_reg_map.at(node.expr1()->id());
  reg_id_t rhs_id = -1;

  auto hint_tag_id = _contexts.back().new_hint_tag_id();
  Label then_label(LabelHint::kLazyThen, hint_tag_id);
  Label else_label(LabelHint::kLazyElse, hint_tag_id);
  Label exit_label(LabelHint::kLazyExit, hint_tag_id);

  CondBranchInst lineC;
  lineC.cond = lhs_id;
  lineC.true_label = then_label;
  lineC.false_label = else_label;
  _contexts.back().push_instruction(std::move(lineC));

  _contexts.back().init_and_start_new_block(then_label);

  if(node.oper() == ast::Operator::kLogicalAnd) {
    node.expr2()->accept(*this);
    rhs_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  }

  BranchInst lineB;
  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  Label then_from_label = _contexts.back().basic_block_packs.back().label;

  _contexts.back().init_and_start_new_block(else_label);

  if(node.oper() == ast::Operator::kLogicalOr) {
    node.expr2()->accept(*this);
    rhs_id = _contexts.back().node_reg_map.at(node.expr2()->id());
  }

  lineB.label = exit_label;
  _contexts.back().push_instruction(std::move(lineB));

  Label else_from_label = _contexts.back().basic_block_packs.back().label;

  _contexts.back().init_and_start_new_block(exit_label);

  auto res_id = _contexts.back().new_reg_id();
  PhiInst lineP;
  lineP.dst = res_id;
  lineP.type = IRType(_type_pool->make_bool());

  switch(node.oper()) {
  case ast::Operator::kLogicalAnd: {
    // %res = phi i1 [%rhs, %laz.then], [0, %laz.else]
    lineP.incoming.emplace_back(Operand::make_reg(rhs_id, IRType(_type_pool->make_bool())), then_from_label);
    lineP.incoming.emplace_back(Operand::make_imm(0, IRType(_type_pool->make_bool())), else_from_label);
  } break;
  case ast::Operator::kLogicalOr: {
    // %res = phi i1 [1, %laz.then], [%rhs, %laz.else]
    lineP.incoming.emplace_back(Operand::make_imm(1, IRType(_type_pool->make_bool())), then_from_label);
    lineP.incoming.emplace_back(Operand::make_reg(rhs_id, IRType(_type_pool->make_bool())), else_from_label);
  } break;
  default: throw std::runtime_error("Unrecognized operator in LazyBooleanExpression");
  }

  _contexts.back().push_instruction(std::move(lineP));

  if(node.need_addr()) {
    res_id = store_into_memory(res_id, IRType(_type_pool->make_bool()));
  }

  // record res
  _contexts.back().node_reg_map.emplace(node.id(), res_id);
}

}

















