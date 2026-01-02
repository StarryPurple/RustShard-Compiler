#include "IR_generator.h"

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

  std::string to_str() const {
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
  int _next_reg_id = 0;
  std::vector<BasicBlockPack> basic_block_packs;
  std::vector<std::unique_ptr<Instruction>> instructions;
  // result of node with this node id is in which register
  std::unordered_map<int, int> node_reg_map;
  // which register records the address of variable in memory
  std::unordered_map<StringT, int> variable_addr_reg_map;
  FunctionPack function_pack;

  int alloca_reg_id() { return _next_reg_id++; }
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
  std::string ident = "str_literal-" + std::to_string(_string_literal_pool.size());
  _string_literal_pool.emplace(literal, ident);
  _ir_pack->static_packs.push_back(StaticPack{.ident = ident, .literal = literal});
  return ident;
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
    // var_ptr-reg-map["self"] = x1

    int reg_id1 = _contexts.back().alloca_reg_id();

    AllocaInst lineA;
    lineA.dst_name = std::to_string(reg_id1); // x1
    lineA.type = ty;
    _contexts.back().instructions.emplace_back(std::make_unique<AllocaInst>(lineA));

    StoreInst lineB;
    lineB.is_instant = false;
    lineB.value_type = ty;
    lineB.ptr_type = ty.get_ref(_type_pool);
    lineB.value_name = reg_name0;
    lineB.ptr_name = lineA.dst_name;
    _contexts.back().instructions.emplace_back(std::make_unique<StoreInst>(lineB));

    _contexts.back().variable_addr_reg_map.emplace("self", reg_id1);

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
      int reg_id1 = _contexts.back().alloca_reg_id();

      AllocaInst lineA;
      lineA.dst_name = std::to_string(reg_id1); // x1
      lineA.type = ty;
      _contexts.back().instructions.emplace_back(std::make_unique<AllocaInst>(lineA));

      StoreInst lineB;
      lineB.is_instant = false;
      lineB.value_type = ty;
      lineB.ptr_type = ty.get_ref(_type_pool);
      lineB.value_name = reg_name0;
      lineB.ptr_name = lineA.dst_name;
      _contexts.back().instructions.emplace_back(std::make_unique<StoreInst>(lineB));

      _contexts.back().variable_addr_reg_map.emplace(pat->ident(), reg_id1);
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
    if(!node.body_opt()->stmts_opt() || !node.body_opt()->stmts_opt()->expr_opt()) {
      ReturnInst lineA;
      lineA.ret_type = IRType(_type_pool->make_unit()); // redundant.
      lineA.ret_val = "";
      _contexts.back().instructions.emplace_back(std::make_unique<ReturnInst>(lineA));
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

void IRGenerator::preVisit(ast::LiteralExpression &node) {
  std::string reg_ident;
  if(node.prime() == stype::TypePrime::kStr) {
    auto literal = std::get<std::string>(node.spec_value());
    reg_ident = use_string_literal(literal);
  }
}

void IRGenerator::postVisit(ast::CallExpression &node) {
  auto func_tp = node.expr()->get_type().get<stype::FunctionType>();
}




}