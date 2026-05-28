#ifndef RUST_SHARD_IR_GENERATOR_H
#define RUST_SHARD_IR_GENERATOR_H

#include "frontend/syntax_check.hpp"
#include "ir_pack.hpp"

namespace rshard::ir {
// uses some stype TypePtr. Please ensure that the type pool is still valid.
class IRGenerator: public ast::ScopedVisitor {
public:
  explicit IRGenerator(stype::TypePool* type_pool): _type_pool(type_pool) {}
  ~IRGenerator() override = default;

  IrPack release() { return std::move(_ir_pack); }

  void preVisit(ast::ConstantItem& node) override;
  void postVisit(ast::ConstantItem& node) override;

  void preVisit(ast::Function& node) override;
  void postVisit(ast::Function& node) override;

  void preVisit(ast::StructStruct& node) override;

  // pass determined types.

  void preVisit(ast::ArithmeticOrLogicalExpression& node) override;
  void preVisit(ast::ComparisonExpression& node) override;
  void preVisit(ast::CompoundAssignmentExpression& node) override;

  void preVisit(ast::LiteralExpression& node) override;
  void postVisit(ast::LiteralExpression& node) override;

  void preVisit(ast::CallExpression& node) override;
  void postVisit(ast::CallExpression& node) override;
  void preVisit(ast::MethodCallExpression& node) override;
  void postVisit(ast::MethodCallExpression& node) override;

  void visit(ast::LetStatement& node) override;

  void preVisit(ast::AssignmentExpression& node) override;
  void postVisit(ast::AssignmentExpression& node) override;

  void preVisit(ast::BorrowExpression& node) override;
  void postVisit(ast::BorrowExpression& node) override;
  void postVisit(ast::DereferenceExpression& node) override;
  void postVisit(ast::PathInExpression& node) override;
  void preVisit(ast::IndexExpression& node) override;
  void postVisit(ast::IndexExpression& node) override;
  void postVisit(ast::TypeCastExpression& node) override;

  void postVisit(ast::NegationExpression& node) override;
  void visit(ast::ArithmeticOrLogicalExpression& node) override;
  void postVisit(ast::ComparisonExpression& node) override;
  void visit(ast::CompoundAssignmentExpression& node) override;
  void preVisit(ast::FieldExpression& node) override;
  void postVisit(ast::FieldExpression& node) override;
  void preVisit(ast::GroupedExpression& node) override;
  void postVisit(ast::GroupedExpression& node) override;
  void visit(ast::StructExpression& node) override;
  void visit(ast::ArrayExpression& node) override;
  void preVisit(ast::BlockExpression& node) override;
  void postVisit(ast::BlockExpression& node) override;

  // branch

  void postVisit(ast::Conditions& node) override;
  void visit(ast::IfExpression& node) override;
  void visit(ast::PredicateLoopExpression& node) override;
  void visit(ast::InfiniteLoopExpression& node) override;

  void postVisit(ast::BreakExpression& node) override;
  void postVisit(ast::ContinueExpression& node) override;
  void preVisit(ast::ReturnExpression& node) override;
  void postVisit(ast::ReturnExpression& node) override;

  void visit(ast::LazyBooleanExpression& node) override;


  // no need to look at type declarations.
  // use intended blank implementation.

  void visit(ast::ParenthesizedType& node) override {}
  void visit(ast::TupleType& node) override {}
  void visit(ast::ReferenceType& node) override {}
  void visit(ast::ArrayType& node) override {}
  void visit(ast::SliceType& node) override {}
  void visit(ast::TypePath& node) override {}

private:
  struct FunctionContext;

  bool _is_in_const = false;

  stype::TypePool* _type_pool;
  IrPack _ir_pack;
  std::unordered_map<StringT, std::string> _string_literal_pool; // StringLiteral -> allocated global variable name
  std::vector<FunctionContext> _contexts;

  std::string use_string_literal(StringT literal) {
    if(auto it = _string_literal_pool.find(literal); it != _string_literal_pool.end())
      return it->second;
    std::string ident = ".str" + std::to_string(_string_literal_pool.size());
    _string_literal_pool.emplace(literal, ident);
    _ir_pack.static_packs.push_back(StaticPack{.ident = ident, .literal = literal});
    return ident;
  }

  static std::string mangle_method(stype::TypePtr impl_type, const std::string& func_name) {
    // doesn't allow to start as number.
    // return "_" + utils::to_base62(impl_type->hash()) + "<" + impl_type->to_string() + ">::" + func_name;
    return "_" + utils::to_base62(impl_type->hash()) + "_" + func_name;
  }

  IrType wrap_ref(IrType type) const {
    return type.get_ref(_type_pool);
  }

  // returns ptr_id
  reg_id_t store_into_memory(reg_id_t obj_id, IrType obj_ty, ast::node_id_t node_id) {
    auto ptr_id = _contexts.back().get_res_ptr_id(node_id, obj_ty);
    StoreInst lineS;
    lineS.ptr = Operand::make_reg(ptr_id, wrap_ref(obj_ty));
    lineS.value = Operand::make_reg(obj_id, obj_ty);
    _contexts.back().push_instruction(std::move(lineS));
    return ptr_id;
  }

  // returns obj_id
  reg_id_t load_from_memory(reg_id_t ptr_id, IrType obj_ty) {
    auto obj_id = _contexts.back().new_reg_id();
    LoadInst lineL;
    lineL.dst = obj_id;
    lineL.ptr = Operand::make_reg(ptr_id, wrap_ref(obj_ty));
    lineL.load_type = obj_ty;
    _contexts.back().push_instruction(std::move(lineL));
    // _contexts.back().add_reg_hint_from(obj_id, ptr_id, "value");
    return obj_id;
  }

  std::optional<reg_id_t> inplace_at(ast::node_id_t node_id) {
    auto &map = _contexts.back().inplace_node_ptr_map;
    if(auto it = map.find(node_id); it != map.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  // With dst and src holding *T ptrs, do *dst <- *src.
  // Will check whether dst == src. At this case, do nothing.
  //
  // As a special circumstance, the following usage:
  //   (already have src, irty)
  //   dst = new_reg; dst = alloca irty;
  //   deep_copy(dst, src, irty);
  // is equivalent to
  //   tmp = load_from_memory(src, irty);
  //   dst = store_into_memory(tmp, irty); // dst newly allocated here.
  // but with no internal object (tmp) in registers.
  // load_from_memory/store_into_memory is designed to allocate one reg,
  // so the equivalence does not hold without "dst = new_reg" line.
  void deep_copy(reg_id_t dst, reg_id_t src, IrType irty) {
    if(dst == src) return;
    auto ptrTy = wrap_ref(irty);
    if(irty.type()->need_indirect_pass()) {
      // call void @memcpy(ptr %dst, ptr %src, i64 %size, i1 false)
      CallInst lineC;
      lineC.ret_type = IrType(_type_pool->make_unit());
      lineC.func_name = "memcpy";
      lineC.args.push_back(Operand::make_reg(dst, ptrTy));
      lineC.args.push_back(Operand::make_reg(src, ptrTy));
      lineC.args.push_back(Operand::make_imm(irty.size(), IrType(_type_pool->make_i64())));
      _contexts.back().push_instruction(std::move(lineC));
    } else {
      auto obj_id = _contexts.back().new_reg_id();
      LoadInst lineL;
      lineL.dst = obj_id;
      lineL.ptr = Operand::make_reg(src, ptrTy);
      lineL.load_type = irty;
      _contexts.back().push_instruction(std::move(lineL));

      StoreInst lineS;
      lineS.ptr = Operand::make_reg(dst, ptrTy);
      lineS.value = Operand::make_reg(obj_id, irty);
      _contexts.back().push_instruction(std::move(lineS));
    }
  }


  // Clone the value stored in src(T*) to another static allocated space.
  // Return the addr of that space.
  reg_id_t clone_to_static(reg_id_t src, IrType irty) {
    reg_id_t dst = _contexts.back().static_allocation(irty);
    deep_copy(dst, src, irty);
    return dst;
  }


  struct FunctionContext {
    bool is_unreachable = false;
    reg_id_t _next_reg_id = 0;
    block_id_t _next_label_hint_id = 0;
    reg_id_t _static_alloc_count = 0;
    std::vector<BasicBlockPack> basic_block_packs;
    std::vector<std::unique_ptr<Instruction>> instructions;
    // result of node with this node id is in which register
    // break/return result is also stored.
    std::unordered_map<ast::node_id_t, reg_id_t> node_reg_map;
    // which register records the address of variable in memory, and the type of the variable
    std::vector<std::unordered_map<StringT, std::pair<reg_id_t, IrType>>> variable_addr_reg_maps;
    FunctionPack function_pack;

    struct LoopContext {
      Label jump_label; // cond for while, body for loop
      Label exit_label; // exit for both while and loop
      reg_id_t res_ptr_id;
    };

    std::vector<LoopContext> loop_contexts;

    // for in-place construction.
    // mapping: from node id to the given value ptr id.
    // Now only urges arrays to construct in-place.
    std::unordered_map<ast::node_id_t, reg_id_t> inplace_node_ptr_map;

    reg_id_t new_reg_id() { return _next_reg_id++; }
    hint_id_t new_label_hint_id() { return _next_label_hint_id++; }

    // if not assigned, alloc one via @new_reg_id.
    // Must be an expression node. The irtype is the object type (not the pointer)
    reg_id_t get_res_ptr_id(ast::node_id_t node_id, IrType irtype) {
      if(auto it = inplace_node_ptr_map.find(node_id); it != inplace_node_ptr_map.end()) {
        return it->second;
      }
      reg_id_t res_ptr_id = new_reg_id();
      AllocaInst lineA;
      lineA.type = irtype;
      lineA.dst = res_ptr_id;
      push_instruction(std::move(lineA));
      return res_ptr_id;
    }

    void init_and_start_new_block(Label& label) {
      is_unreachable = false;
      basic_block_packs.back().instructions = std::move(instructions);
      label.block_id = static_cast<int>(basic_block_packs.size()); // set label id.
      basic_block_packs.emplace_back(BasicBlockPack{.label = label});
    }

    void add_reg_hint(reg_id_t id, const std::string& hint) {
      // might override old hint.
      function_pack.hints.emplace(id, hint);
    }

    /*
    void add_reg_hint_from(int id, int base_id, const std::string &append_hint) {
      auto it = function_pack.hints.find(base_id);
      std::string hint;
      if(it != function_pack.hints.end()) hint = it->second + ".";
      hint += append_hint;
      add_reg_hint(id, hint);
    }
    */
    template <class Inst> requires std::is_base_of_v<Instruction, Inst>
    void push_instruction(Inst&& inst) {
      if(is_unreachable) return;
      auto inst_ptr = std::make_unique<Inst>(std::forward<Inst>(inst));
      // auto raw_ptr = inst_ptr.get();
      instructions.emplace_back(std::move(inst_ptr));
    }

    reg_id_t static_allocation(IrType irty) {
      // Add this allocation to the beginning of the function
      // to avoid multiple stack usage in loops.
      reg_id_t addr_id = new_reg_id();
      AllocaInst lineA;
      lineA.dst = addr_id;
      lineA.type = irty;

      // Add to a place more global than current block.
      // The entry block is the only always-reliable helper.
      auto &instrs
         = (basic_block_packs.size() == 1)
         ? instructions : basic_block_packs.front().instructions;
      // For prettiness, put them at the beginning.
      instrs.insert(instrs.begin() + (_static_alloc_count++), std::make_unique<AllocaInst>(std::move(lineA)));

      return addr_id;
    }

    std::pair<int, IrType> find_variable(const StringT& ident) {
      for(auto rit = variable_addr_reg_maps.rbegin(); rit != variable_addr_reg_maps.rend(); ++rit) {
        if(auto it = rit->find(ident); it != rit->end()) return it->second;
      }
      throw std::runtime_error("Variable not found");
    }
  };
};
} // namespace rshard

#endif // RUST_SHARD_IR_GENERATOR_H
