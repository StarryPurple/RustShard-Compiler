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
  // clear everything lower than function pack (maybe unnecessary)
  _basic_blocks.clear();
  _instructions.clear();
  // basic block entrance:
  _basic_blocks.push_back(BasicBlockPack{
    .label = "_entry",
  });

  auto info = find_symbol(node.ident());
  auto func_tp = info->type.get<stype::FunctionType>();
  std::vector<std::pair<StringT, IRType>> params;
  if(func_tp->self_type_opt()) {
    throw std::runtime_error("Unimplemented method IR");
  }
  for(int i = 0; i < func_tp->params().size(); ++i) {
    auto param = node.params_opt()->func_params()[i].get();
    int reg_id = _next_reg_id++;
    if(auto p = dynamic_cast<ast::FunctionParamPattern*>(param)) {
      auto pat = dynamic_cast<ast::IdentifierPattern*>(p->pattern().get());
      if(!pat) {
        throw std::runtime_error("Func param too complicated");
      }
      // Ty %x0 (name)
      // allocate in memory.
      // %x1 = alloca Ty
      // store Ty %x0, Ty* %x1
      // var_ptr-reg-map[name] = x1
      AllocaInst lineA;
      lineA.reg = std::to_string(_next_reg_id++);
      lineA.type = IRType(func_tp->params()[i]);
      _instructions.emplace_back(std::make_unique<AllocaInst>(lineA));
      _variable_addr_reg_map.emplace(pat->ident(), reg_id);
    }
    params.emplace_back(std::to_string(reg_id), func_tp->params()[i]);
  }
  _function_packs.emplace_back(FunctionPack{
    .ident = node.ident(),
    .ret_type = IRType(func_tp->ret_type()),
    .params = params
  });
  ScopedVisitor::preVisit(node);
}

void IRGenerator::postVisit(ast::Function &node) {
  ScopedVisitor::postVisit(node);
  _ir_pack->function_packs.push_back(std::move(_function_packs.back()));
  _function_packs.pop_back();
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
  _instructions.push_back(std::make_unique<CallInst>(CallInst{
    .dst_name =
  }));
}




}