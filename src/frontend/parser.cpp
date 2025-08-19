#include "parser.h"

namespace insomnia::rust_shard::ast {

class Parser::Context {
  friend Backtracker;
  std::vector<Token> _tokens;
  std::size_t _pos = 0;
public:
  Context() = default;
  template <class T>
  explicit Context(T &&tokens, std::size_t pos = 0)
    : _tokens(std::forward<T>(tokens)), _pos(pos) {}
  // returns a default token with token_type == INVALID if fails.
  [[nodiscard]] Token peek(std::size_t diff = 1) const {
    if(_pos + diff >= _tokens.size())
      return Token{};
    return _tokens[_pos + diff];
  }
  // returns a default token with token_type == INVALID if fails.
  [[nodiscard]] Token current() const {
    if(_pos >= _tokens.size())
      return Token{};
    return _tokens[_pos];
  }
  void consume() {
    if(_pos >= _tokens.size())
      throw std::runtime_error("ASTContext consume out of range.");
    _pos++;
  }
  [[nodiscard]] bool is_peek_safe(std::size_t diff = 1) const { return _pos + diff < _tokens.size(); }
  [[nodiscard]] bool empty() const { return _pos >= _tokens.size(); }
  void reset() { _pos = 0; }
};

class Parser::Backtracker {
  Context &_ast_ctx;
  std::size_t _pos;
  bool _commited;
public:
  explicit Backtracker(Context &ast_ctx)
  : _ast_ctx(ast_ctx), _pos(ast_ctx._pos), _commited(false) {}
  ~Backtracker() {
    if(!_commited) _ast_ctx._pos = _pos;
  }
  void commit() { _commited = true; }
};

void Parser::parse(Lexer &lexer) {
  _ast_ctx = std::make_unique<Context>(lexer.release());
  _crate = parse_crate();
  _is_good = static_cast<bool>(_crate);
}

std::unique_ptr<Crate> Parser::parse_crate() {
  // The beginning. No need for backtracker.
  // Item*
  std::vector<std::unique_ptr<Item>> items;
  while(!_ast_ctx->empty()) {
    if(auto item = parse_item(); item) {
      items.push_back(std::move(item));
    } else {
      // Not allowed to fail parsing when still something's left.
      return nullptr;
    }
  }
  return std::make_unique<Crate>(std::move(items));
}

std::unique_ptr<Item> Parser::parse_item() {
  Backtracker tracker(*_ast_ctx);
  // VisItem
  if(auto vis_item = parse_vis_item(); vis_item) {
    tracker.commit();
    return std::make_unique<Item>(std::move(vis_item));
  }
  return nullptr;
}


std::unique_ptr<VisItem> Parser::parse_vis_item() {
  Backtracker tracker(*_ast_ctx);
  // Function
  if(auto f = parse_function()) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(f));
  }
  // Struct
  if(auto s = parse_struct()) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(s));
  }
  // Enumeration
  if(auto e = parse_enumeration()) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(e));
  }
  // ConstantItem
  if(auto c = parse_constant_item()) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(c));
  }
  // Trait
  if(auto t = parse_trait()) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(t));
  }
  // Implementation
  if(auto i = parse_implementation()) {
    tracker.commit();
    return std::make_unique<VisItem>(std::move(i));
  }
  return nullptr;
}

std::unique_ptr<Function> Parser::parse_function() {
  Backtracker tracker(*_ast_ctx);
  // "const"?
  bool is_const = false;
  if(_ast_ctx->current().token_type == TokenType::CONST) {
    is_const = true;
    _ast_ctx->consume();
  }
  // "fn"
  if(_ast_ctx->current().token_type != TokenType::FN) {
    return nullptr;
  }
  _ast_ctx->consume();
  // IDENTIFIER
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  std::string_view ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  // '('
  if(_ast_ctx->current().token_type != TokenType::L_PARENTHESIS) {
    return nullptr;
  }
  _ast_ctx->consume();
  // FunctionParameters?
  auto params_opt = parse_function_parameters();
  // ')'
  if(_ast_ctx->current().token_type != TokenType::R_PARENTHESIS) {
    return nullptr;
  }
  _ast_ctx->consume();
  // "->" Type
  std::unique_ptr<Type> type_opt;
  if(_ast_ctx->current().token_type == TokenType::R_ARROW) {
    _ast_ctx->consume();
    type_opt = parse_type();
    if(!type_opt) {
      return nullptr;
    }
  }
  // BlockExpression | ';'
  std::unique_ptr<BlockExpression> block_expr_opt;
  if(_ast_ctx->current().token_type != TokenType::SEMI) {
    block_expr_opt = parse_block_expression();
    if(!block_expr_opt) {
      return nullptr;
    }
  }
  tracker.commit();
  return std::make_unique<Function>(
    is_const, ident, std::move(params_opt), std::move(type_opt), std::move(block_expr_opt)
  );
}

std::unique_ptr<FunctionParameters> Parser::parse_function_parameters() {
  {
    // SelfParam ','?
    Backtracker tracker(*_ast_ctx);
    if(auto self_param = parse_self_param()) {
      if(_ast_ctx->current().token_type == TokenType::COMMA) {
        _ast_ctx->consume();
      }
      if(_ast_ctx->current().token_type == TokenType::R_PARENTHESIS) {
        tracker.commit();
        return std::make_unique<FunctionParameters>(
          std::move(self_param), std::vector<std::unique_ptr<FunctionParam>>{}
        );
      }
    }
    // restore consumption try.
  }
  {
    Backtracker tracker(*_ast_ctx);
    // (SelfParam ',')? FunctionParam (',' FunctionParam)* ','?
    auto self_param = parse_self_param();
    if(self_param) {
      if(_ast_ctx->current().token_type != TokenType::COMMA) {
        return nullptr;
      }
      _ast_ctx->consume();
    }
    std::vector<std::unique_ptr<FunctionParam>> params;
    if(auto param = parse_function_param(); !param) {
      return nullptr;
    } else {
      params.push_back(std::move(param));
    }
    while(_ast_ctx->current().token_type == TokenType::COMMA) {
      _ast_ctx->consume();
      if(_ast_ctx->current().token_type == TokenType::R_PARENTHESIS) {
        break;
      }
      if(auto param = parse_function_param(); !param) {
        return nullptr;
      } else {
        params.push_back(std::move(param));
      }
    }
    tracker.commit();
    return std::make_unique<FunctionParameters>(
      std::move(self_param), std::move(params)
    );
  }
}

std::unique_ptr<FunctionParam> Parser::parse_function_param() {
  Backtracker tracker(*_ast_ctx);
  // FunctionParamPattern
  if(auto f = parse_function_param_pattern()) {
    tracker.commit();
    return std::make_unique<FunctionParam>(std::move(f));
  }
  // Type
  if(auto t = parse_type()) {
    tracker.commit();
    return std::make_unique<FunctionParam>(std::move(t));
  }
  return nullptr;
}

std::unique_ptr<FunctionParamPattern> Parser::parse_function_param_pattern() {
  Backtracker tracker(*_ast_ctx);
  // PatternNoTopAlt
  auto p = parse_pattern_no_top_alt();
  if(!p) {
    return nullptr;
  }
  // ':'
  if(_ast_ctx->current().token_type != TokenType::COLON) {
    return nullptr;
  }
  _ast_ctx->consume();
  // Type
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<FunctionParamPattern>(std::move(p), std::move(t));
}

std::unique_ptr<FunctionParamType> Parser::parse_function_param_type() {

}

std::unique_ptr<SelfParam> Parser::parse_self_param() {
  Backtracker tracker(*_ast_ctx);
  // '&'?
  bool is_ref = false;
  if(_ast_ctx->current().token_type == TokenType::AND) {
    is_ref = true;
    _ast_ctx->consume();
  }
  // "mut"?
  bool is_mut = false;
  if(_ast_ctx->current().token_type == TokenType::MUT) {
    is_mut = true;
    _ast_ctx->consume();
  }
  // "self"
  if(_ast_ctx->current().token_type != TokenType::SELF_OBJECT) {
    return nullptr;
  }
  _ast_ctx->consume();
  // (':' Type)?
  std::unique_ptr<Type> t;
  if(_ast_ctx->current().token_type == TokenType::COLON) {
    _ast_ctx->consume();
    t = parse_type();
    if(!t) {
      return nullptr;
    }
  }
  tracker.commit();
  return std::make_unique<SelfParam>(is_ref, is_mut, std::move(t));
}

std::unique_ptr<Type> Parser::parse_type() {
  Backtracker tracker(*_ast_ctx);
  // TypeNoBounds
  if(auto t = parse_type_no_bounds()) {
    tracker.commit();
    return std::make_unique<Type>(std::move(t));
  }
  return nullptr;
}

std::unique_ptr<TypeNoBounds> Parser::parse_type_no_bounds() {
  Backtracker tracker(*_ast_ctx);
  // ParenthesizedType
  if(auto p = parse_parenthesized_type()) {
    tracker.commit();
    return std::make_unique<TypeNoBounds>(std::move(p));
  }
  // TupleType
  if(auto t = parse_tuple_type()) {
    tracker.commit();
    return std::make_unique<TypeNoBounds>(std::move(t));
  }
  // ReferenceType
  if(auto r = parse_reference_type()) {
    tracker.commit();
    return std::make_unique<TypeNoBounds>(std::move(r));
  }
  // ArrayType
  if(auto a = parse_array_type()) {
    tracker.commit();
    return std::make_unique<TypeNoBounds>(std::move(a));
  }
  // SliceType
  if(auto s = parse_slice_type()) {
    tracker.commit();
    return std::make_unique<TypeNoBounds>(std::move(s));
  }
  return nullptr;
}

std::unique_ptr<ParenthesizedType> Parser::parse_parenthesized_type() {
  Backtracker tracker(*_ast_ctx);
  // '('
  if(_ast_ctx->current().token_type != TokenType::L_PARENTHESIS) {
    return nullptr;
  }
  // Type
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  // ')'
  if(_ast_ctx->current().token_type != TokenType::R_PARENTHESIS) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<ParenthesizedType>(std::move(t));
}

std::unique_ptr<TupleType> Parser::parse_tuple_type() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Type>> types;
  // '('
  if(_ast_ctx->current().token_type != TokenType::L_PARENTHESIS) {
    return nullptr;
  }
  _ast_ctx->consume();
  // empty?
  if(_ast_ctx->current().token_type == TokenType::R_PARENTHESIS) {
    _ast_ctx->consume();
    tracker.commit();
    return std::make_unique<TupleType>(std::move(types));
  }
  // (Type ',')+ Type?
  if(auto t = parse_type(); !t) {
    return nullptr;
  } else {
    types.push_back(std::move(t));
  }
  if(_ast_ctx->current().token_type != TokenType::COMMA) {
    return nullptr;
  }
  _ast_ctx->consume();
  while(_ast_ctx->current().token_type != TokenType::R_PARENTHESIS) {
    auto t = parse_type();
    if(!t) {
      return nullptr;
    }
    types.push_back(std::move(t));
    if(_ast_ctx->current().token_type == TokenType::COMMA) {
      _ast_ctx->consume();
    } else if(_ast_ctx->current().token_type != TokenType::R_PARENTHESIS) {
      return nullptr;
    }
  }
  // ')'
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<TupleType>(std::move(types));
}

std::unique_ptr<ReferenceType> Parser::parse_reference_type() {
  Backtracker tracker(*_ast_ctx);
  // '&'
  if(_ast_ctx->current().token_type != TokenType::AND) {
    return nullptr;
  }
  _ast_ctx->consume();
  // "mut"?
  bool is_mut = false;
  if(_ast_ctx->current().token_type == TokenType::MUT) {
    is_mut = true;
    _ast_ctx->consume();
  }
  // TypeNoBounds
  auto t = parse_type_no_bounds();
  if(!t) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<ReferenceType>(is_mut, std::move(t));
}

std::unique_ptr<ArrayType> Parser::parse_array_type() {
  Backtracker tracker(*_ast_ctx);
  // '['
  if(_ast_ctx->current().token_type != TokenType::L_SQUARE_BRACKET) {
    return nullptr;
  }
  _ast_ctx->consume();
  // Type
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  // ';'
  if(_ast_ctx->current().token_type != TokenType::SEMI) {
    return nullptr;
  }
  _ast_ctx->consume();
  // Expression
  auto e = parse_expression();
  if(!e) {
    return nullptr;
  }
  // ']'
  if(_ast_ctx->current().token_type != TokenType::R_SQUARE_BRACKET) {
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<ArrayType>(std::move(t), std::move(e));
}

std::unique_ptr<SliceType> Parser::parse_slice_type() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::L_SQUARE_BRACKET) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  if(_ast_ctx->current().token_type != TokenType::R_SQUARE_BRACKET) {
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<SliceType>(std::move(t));
}

std::unique_ptr<Struct> Parser::parse_struct() {
  Backtracker tracker(*_ast_ctx);
  auto ss = parse_struct_struct();
  if(!ss) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<Struct>(std::move(ss));
}

std::unique_ptr<StructStruct> Parser::parse_struct_struct() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::STRUCT) {
    return nullptr;
  }
  _ast_ctx->consume();
  std::string_view ident;
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();

  if(_ast_ctx->current().token_type == TokenType::SEMI) {
    tracker.commit();
    return std::make_unique<StructStruct>(ident, std::unique_ptr<StructFields>());
  }
  if(_ast_ctx->current().token_type != TokenType::L_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto sf = parse_struct_fields();
  if(!sf) {
    return nullptr;
  }
  if(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<StructStruct>(ident, std::move(sf));
}

std::unique_ptr<StructFields> Parser::parse_struct_fields() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<StructField>> fields;
  if(auto sf = parse_struct_field(); !sf) {
    return nullptr;
  } else {
    fields.push_back(std::move(sf));
  }
  while(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    if(_ast_ctx->current().token_type != TokenType::SEMI) {
      return nullptr;
    }
    _ast_ctx->consume();
    if(_ast_ctx->current().token_type == TokenType::R_CURLY_BRACE) {
      break;
    }
    auto sf = parse_struct_field();
    if(!sf) {
      return nullptr;
    }
    fields.push_back(std::move(sf));
  }
  tracker.commit();
  return std::make_unique<StructFields>(std::move(fields));
}

std::unique_ptr<StructField> Parser::parse_struct_field() {
  Backtracker tracker(*_ast_ctx);
  std::string_view ident;
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::COLON) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<StructField>(ident, std::move(t));
}

std::unique_ptr<Enumeration> Parser::parse_enumeration() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::ENUM) {
    return nullptr;
  }
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  std::string_view ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::L_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  std::unique_ptr<EnumItems> ei;
  if(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    ei = parse_enum_items();
    if(!ei) {
      return nullptr;
    }
  }
  if(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<Enumeration>(ident, std::move(ei));
}

std::unique_ptr<EnumItems> Parser::parse_enum_items() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<EnumItem>> items;
  if(auto ei = parse_enum_item(); !ei) {
    return nullptr;
  } else {
    items.push_back(std::move(ei));
  }
  while(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    if(_ast_ctx->current().token_type != TokenType::SEMI) {
      return nullptr;
    }
    _ast_ctx->consume();
    if(_ast_ctx->current().token_type == TokenType::R_CURLY_BRACE) {
      break;
    }
    auto ei = parse_enum_item();
    if(!ei) {
      return nullptr;
    }
    items.push_back(std::move(ei));
  }
  tracker.commit();
  return std::make_unique<EnumItems>(std::move(items));
}

std::unique_ptr<EnumItem> Parser::parse_enum_item() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  auto dis = parse_enum_item_discriminant();
  tracker.commit();
  return std::make_unique<EnumItem>(ident, std::move(dis));
}

std::unique_ptr<EnumItemDiscriminant> Parser::parse_enum_item_discriminant() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::EQ) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto e = parse_expression();
  if(!e) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<EnumItemDiscriminant>(std::move(e));
}

std::unique_ptr<ConstantItem> Parser::parse_constant_item() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::CONST) {
    return nullptr;
  }
  _ast_ctx->consume();
  std::string_view ident;
  if(_ast_ctx->current().token_type == TokenType::IDENTIFIER ||
    _ast_ctx->current().token_type == TokenType::UNDERSCORE) {
    ident = _ast_ctx->current().lexeme;
  } else {
    return nullptr;
  }
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::COLON) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  std::unique_ptr<Expression> e;
  if(_ast_ctx->current().token_type == TokenType::EQ) {
    _ast_ctx->consume();
    e = parse_expression();
    if(!e) {
      return nullptr;
    }
  }
  if(_ast_ctx->current().token_type != TokenType::SEMI) {
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<ConstantItem>(ident, std::move(t), std::move(e));
}

std::unique_ptr<Trait> Parser::parse_trait() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::TRAIT) {
    return nullptr;
  }
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::L_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  std::vector<std::unique_ptr<AssociatedItem>> items;
  while(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    auto ai = parse_associated_item();
    if(!ai) {
      return nullptr;
    }
    items.push_back(std::move(ai));
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<Trait>(ident, std::move(items));
}

std::unique_ptr<AssociatedItem> Parser::parse_associated_item() {
  Backtracker tracker(*_ast_ctx);
  if(auto t = parse_type_alias()) {
    tracker.commit();
    return std::make_unique<AssociatedItem>(std::move(t));
  }
  if(auto c = parse_constant_item()) {
    tracker.commit();
    return std::make_unique<AssociatedItem>(std::move(c));
  }
  if(auto f = parse_function()) {
    tracker.commit();
    return std::make_unique<AssociatedItem>(std::move(f));
  }
  return nullptr;
}

std::unique_ptr<AssociatedTypeAlias> Parser::parse_associated_type_alias() {

}

std::unique_ptr<AssociatedConstantItem> Parser::parse_associated_constant_item() {

}

std::unique_ptr<AssociatedFunction> Parser::parse_associated_function() {

}

std::unique_ptr<TypeAlias> Parser::parse_type_alias() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::TYPE) {
    return nullptr;
  }
  _ast_ctx->consume();
  if(_ast_ctx->current().token_type != TokenType::IDENTIFIER) {
    return nullptr;
  }
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  std::unique_ptr<Type> t;
  if(_ast_ctx->current().token_type == TokenType::EQ) {
    _ast_ctx->consume();
    t = parse_type();
    if(!t) {
      return nullptr;
    }
  }
  if(_ast_ctx->current().token_type != TokenType::SEMI) {
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<TypeAlias>(ident, std::move(t));
}

std::unique_ptr<Implementation> Parser::parse_implementation() {
  Backtracker tracker(*_ast_ctx);
  if(auto i = parse_inherent_impl()) {
    tracker.commit();
    return std::make_unique<Implementation>(std::move(i));
  }
  if(auto t = parse_trait_impl()) {
    tracker.commit();
    return std::make_unique<Implementation>(std::move(t));
  }
  return nullptr;
}

std::unique_ptr<InherentImpl> Parser::parse_inherent_impl() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::IMPL) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto t = parse_type();
  if(!t) {
    return nullptr;
  }
  if(_ast_ctx->current().token_type != TokenType::L_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  std::vector<std::unique_ptr<AssociatedItem>> items;
  while(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    auto ai = parse_associated_item();
    if(!ai) {
      return nullptr;
    }
    items.push_back(std::move(ai));
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<InherentImpl>(std::move(t), std::move(items));
}

std::unique_ptr<TraitImpl> Parser::parse_trait_impl() {
  Backtracker tracker(*_ast_ctx);
  if(_ast_ctx->current().token_type != TokenType::IMPL) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto tpp = parse_type_path();
  if(!tpp) {
    return nullptr;
  }
  if(_ast_ctx->current().token_type != TokenType::FOR) {
    return nullptr;
  }
  _ast_ctx->consume();
  auto tp = parse_type();
  if(!tp) {
    return nullptr;
  }
  if(_ast_ctx->current().token_type != TokenType::L_CURLY_BRACE) {
    return nullptr;
  }
  _ast_ctx->consume();
  std::vector<std::unique_ptr<AssociatedItem>> items;
  while(_ast_ctx->current().token_type != TokenType::R_CURLY_BRACE) {
    auto ai = parse_associated_item();
    if(!ai) {
      return nullptr;
    }
    items.push_back(std::move(ai));
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<TraitImpl>(std::move(tpp), std::move(tp), std::move(items));
}

std::unique_ptr<TypePath> Parser::parse_type_path() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<TypePathSegment>> ss;
  if(auto s1 = parse_type_path_segment()) {
    ss.push_back(std::move(s1));
  } else {
    return nullptr;
  }
  while(_ast_ctx->current().token_type == TokenType::PATH_SEP) {
    auto s = parse_type_path_segment();
    if(!s) {
      return nullptr;
    }
    ss.push_back(std::move(s));
  }
  tracker.commit();
  return std::make_unique<TypePath>(std::move(ss));
}

std::unique_ptr<TypePathSegment> Parser::parse_type_path_segment() {
  Backtracker tracker(*_ast_ctx);
  auto p = parse_path_ident_segment();
  if(!p) {
    return nullptr;
  }
  tracker.commit();
  return std::make_unique<TypePathSegment>(std::move(p));
}

std::unique_ptr<PathIdentSegment> Parser::parse_path_ident_segment() {
  Backtracker tracker(*_ast_ctx);
  std::string_view ident;
  switch(_ast_ctx->current().token_type) {
  case TokenType::IDENTIFIER:
  case TokenType::SUPER:
  case TokenType::SELF_OBJECT:
  case TokenType::SELF_TYPE:
  case TokenType::CRATE:
    ident = _ast_ctx->current().lexeme;
    break;
  default:
    return nullptr;
  }
  _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<PathIdentSegment>(ident);
}

std::unique_ptr<Expression> Parser::parse_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExpressionWithoutBlock> Parser::parse_expression_without_block() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<LiteralExpression> Parser::parse_literal_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathExpression> Parser::parse_path_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathInExpression> Parser::parse_path_in_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathExprSegment> Parser::parse_path_expr_segment() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<OperatorExpression> Parser::parse_operator_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<BorrowExpression> Parser::parse_borrow_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<DereferenceExpression> Parser::parse_dereference_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<NegationExpression> Parser::parse_negation_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ArithmeticOrLogicalExpression> Parser::parse_arithmetic_or_logical_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ComparisonExpression> Parser::parse_comparison_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<LazyBooleanExpression> Parser::parse_lazy_boolean_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<TypeCastExpression> Parser::parse_type_cast_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<AssignmentExpression> Parser::parse_assignment_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<CompoundAssignmentExpression> Parser::parse_compound_assignment_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<GroupedExpression> Parser::parse_grouped_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ArrayExpression> Parser::parse_array_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ArrayElements> Parser::parse_array_elements() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExplicitArrayElements> Parser::parse_explicit_array_elements() {

}

std::unique_ptr<RepeatedArrayElements> Parser::parse_repeated_array_elements() {

}

std::unique_ptr<IndexExpression> Parser::parse_index_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<TupleExpression> Parser::parse_tuple_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<TupleElements> Parser::parse_tuple_elements() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<TupleIndexingExpression> Parser::parse_tuple_indexing_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructExpression> Parser::parse_struct_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructExprFields> Parser::parse_struct_expr_fields() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructExprField> Parser::parse_struct_expr_field() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<NamedStructExprField> Parser::parse_named_struct_expr_field() {

}

std::unique_ptr<IndexStructExprField> Parser::parse_index_struct_expr_field() {

}

std::unique_ptr<CallExpression> Parser::parse_call_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<CallParams> Parser::parse_call_params() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<MethodCallExpression> Parser::parse_method_call_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<FieldExpression> Parser::parse_field_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ContinueExpression> Parser::parse_continue_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<BreakExpression> Parser::parse_break_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeExpression> Parser::parse_range_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeExpr> Parser::parse_range_expr() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeFromExpr> Parser::parse_range_from_expr() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeToExpr> Parser::parse_range_to_expr() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeFullExpr> Parser::parse_range_full_expr() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeInclusiveExpr> Parser::parse_range_inclusive_expr() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeToInclusiveExpr> Parser::parse_range_to_inclusive_expr() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ReturnExpression> Parser::parse_return_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<UnderscoreExpression> Parser::parse_underscore_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExpressionWithBlock> Parser::parse_expression_with_block() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<BlockExpression> Parser::parse_block_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<Statements> Parser::parse_statements() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<Statement> Parser::parse_statement() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<EmptyStatement> Parser::parse_empty_statement() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ItemStatement> Parser::parse_item_statement() {

}

std::unique_ptr<LetStatement> Parser::parse_let_statement() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExpressionStatement> Parser::parse_expression_statement() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<LoopExpression> Parser::parse_loop_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<InfiniteLoopExpression> Parser::parse_infinite_loop_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PredicateLoopExpression> Parser::parse_predicate_loop_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<IfExpression> Parser::parse_if_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<Conditions> Parser::parse_conditions() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchExpression> Parser::parse_match_expression() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchArms> Parser::parse_match_arms() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchArm> Parser::parse_match_arm() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchArmGuard> Parser::parse_match_arm_guard() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<Pattern> Parser::parse_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PatternNoTopAlt> Parser::parse_pattern_no_top_alt() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PatternWithoutRange> Parser::parse_pattern_without_range() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<LiteralPattern> Parser::parse_literal_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<IdentifierPattern> Parser::parse_identifier_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<WildcardPattern> Parser::parse_wildcard_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<ReferencePattern> Parser::parse_reference_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPattern> Parser::parse_struct_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPatternElements> Parser::parse_struct_pattern_elements() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPatternFields> Parser::parse_struct_pattern_fields() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPatternField> Parser::parse_struct_pattern_field() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<TuplePattern> Parser::parse_tuple_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<TuplePatternItems> Parser::parse_tuple_pattern_items() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<GroupedPattern> Parser::parse_grouped_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<SlicePattern> Parser::parse_slice_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<SlicePatternItems> Parser::parse_slice_pattern_items() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathPattern> Parser::parse_path_pattern() {
  Backtracker tracker(*_ast_ctx);
  tracker.commit();
  return nullptr;
}

}