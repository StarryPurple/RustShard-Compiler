#include "parser.h"
#include "ast_enums.h"

#include <unordered_map>

#define REPORT_FAILURE_AND_RETURN(FailureMessage) \
  do { \
    _ast_ctx->recordError( \
      std::string("From ") + std::string(__func__) + ": " + FailureMessage \
    ); \
    return nullptr; \
  } while(false)

#define REPORT_PARSE_FAILURE_AND_RETURN() \
  do { \
    REPORT_FAILURE_AND_RETURN( \
      "Unexpected parse failure: Failure in parsing the expected AST node." \
    ); \
  } while(false)

#define REPORT_MISSING_TOKEN_AND_RETURN(ExpectedType) \
  do { \
    REPORT_FAILURE_AND_RETURN( \
      " Unexpected parse failure: Expected puntuation " + \
      #ExpectedType + " not appearing." \
    ); \
  } while(false)

#define EXPECT_CONTEXT_NOT_EMPTY() \
  do { \
    if(_ast_ctx->empty()) { \
      REPORT_FAILURE_AND_RETURN( \
        "Unexpected token drain." \
      ); \
    } \
  } while(false)

#define EXPECT_POINTER_NOT_EMPTY(node_pointer) \
  do { \
    if(!node_pointer) { \
      REPORT_FAILURE_AND_RETURN( \
        "Unexpected parse failure: Failure in parsing the expected AST node." \
      ); \
    } \
  } while(false)

#define CHECK_TOKEN(ExpectedType) \
  (_ast_ctx->current().token_type == TokenType::ExpectedType)

#define EXPECT_TOKEN(ExpectedType) \
  do { \
    if(!CHECK_TOKEN(ExpectedType)) { \
      REPORT_FAILURE_AND_RETURN( \
        "Unexpected token at" + \
        " row:" + std::to_string(_ast_ctx->current().row) + \
        " col:" + std::to_string(_ast_ctx->current().col) + \
        ". Expected " + #ExpectedType + \
        ", but got " + token_type_to_string(_ast_ctx->current().token_type) \
      ); \
    } \
  } while(false)

// expect and consume
#define MATCH_TOKEN(ExpectedType) \
  do { \
    EXPECT_TOKEN(ExpectedType); \
    _ast_ctx->consume(); \
  } while(false)

namespace insomnia::rust_shard::ast {

class Parser::Context {
  friend Backtracker;
  std::vector<Token> _tokens;
  std::size_t _pos = 0;
  std::vector<std::string> _errors;
public:
  Context() = default;
  template <class T>
  explicit Context(T &&tokens, std::size_t pos = 0)
    : _tokens(std::forward<T>(tokens)), _pos(pos) {}
  // returns a default token with token_type == INVALID if fails.
  Token peek(std::size_t diff = 1) const {
    if(_pos + diff >= _tokens.size())
      return Token{};
    return _tokens[_pos + diff];
  }
  // returns a default token with token_type == INVALID if fails.
  Token current() const {
    if(_pos >= _tokens.size())
      return Token{};
    return _tokens[_pos];
  }
  void consume() {
    if(_pos >= _tokens.size())
      throw std::runtime_error("ASTContext consume out of range.");
    _pos++;
  }
  bool empty() const { return _pos >= _tokens.size(); }
  void reset() { _pos = 0; }

  const std::vector<std::string>& errors() const { return _errors; }
  void recordError(std::string &&msg) { _errors.push_back(std::move(msg)); }
};

class Parser::Backtracker {
  Context &_ast_ctx;
  std::size_t _pos, _err_pos;
  bool _commited;
public:
  explicit Backtracker(Context &ast_ctx)
  : _ast_ctx(ast_ctx), _pos(ast_ctx._pos),
  _err_pos(ast_ctx._errors.size()), _commited(false) {}
  ~Backtracker() {
    if(!_commited) rollback();
  }
  void rollback() {
    _ast_ctx._pos = _pos;
    _ast_ctx._errors.resize(_err_pos);
  }
  void commit() { _commited = true; }
};

std::string Parser::error_msg() const {
  std::string res;
  for(const auto &msg: _ast_ctx->errors()) {
    res += msg;
    res += '\n';
  }
  return res;
}

void Parser::parseAll(Lexer &lexer) {
  _ast_ctx = std::make_unique<Context>(lexer.release());
  _crate = parseCrate();
  _is_good = static_cast<bool>(_crate);
}

std::unique_ptr<Crate> Parser::parseCrate() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Item>> items;
  while(!_ast_ctx->empty()) {
    auto i = parseItem();
    EXPECT_POINTER_NOT_EMPTY(i);
    items.push_back(std::move(i));
  }
  tracker.commit();
  return std::make_unique<Crate>(std::move(items));
}

std::unique_ptr<Item> Parser::parseItem() {
  Backtracker tracker(*_ast_ctx);
  auto vi = parseVisItem();
  EXPECT_POINTER_NOT_EMPTY(vi);
  tracker.commit();
  return std::make_unique<Item>(std::move(vi));
}

std::unique_ptr<VisItem> Parser::parseVisItem() {
  Backtracker tracker(*_ast_ctx);
  if(auto f = parseFunction()) {
    tracker.commit();
    return f;
  }
  if(auto t = parseTypeAlias()) {
    tracker.commit();
    return t;
  }
  if(auto s = parseStruct()) {
    tracker.commit();
    return s;
  }
  if(auto e = parseEnumeration()) {
    tracker.commit();
    return e;
  }
  if(auto c = parseConstantItem()) {
    tracker.commit();
    return c;
  }
  if(auto t = parseTrait()) {
    tracker.commit();
    return t;
  }
  if(auto i = parseImplementation()) {
    tracker.commit();
    return i;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<Function> Parser::parseFunction() {
  Backtracker tracker(*_ast_ctx);
  bool is_const = false;
  if(CHECK_TOKEN(TokenType::kConst)) {
    is_const = true;
    _ast_ctx->consume();
  }
  MATCH_TOKEN(TokenType::kFn);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(TokenType::kLParenthesis);
  auto fp_opt = parseFunctionParameters();
  MATCH_TOKEN(TokenType::kRParenthesis);
  std::unique_ptr<Type> t_opt;
  if(CHECK_TOKEN(TokenType::kRArrow)) {
    _ast_ctx->consume();
    t_opt = parseType();
    EXPECT_POINTER_NOT_EMPTY(t_opt);
  }
  std::unique_ptr<BlockExpression> be_opt;
  if(CHECK_TOKEN(TokenType::kSemi)) {
    _ast_ctx->consume();
  } else {
    be_opt = parseBlockExpression();
    EXPECT_POINTER_NOT_EMPTY(be_opt);
  }
  tracker.commit();
  return std::make_unique<Function>(
    is_const, ident, std::move(fp_opt), std::move(t_opt), std::move(be_opt)
  );
}

std::unique_ptr<FunctionParameters> Parser::parseFunctionParameters() {
  Backtracker tracker(*_ast_ctx);
  // SP ','? {')'} | (SP ',')? FP (',' FP)* ','? {')'}
  auto sp = parseSelfParam();
  std::vector<std::unique_ptr<FunctionParam>> fps;
  if(sp) {
    bool has_comma = CHECK_TOKEN(TokenType::kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(TokenType::kRParenthesis)) {
      // SP ','? {')'}
      tracker.commit();
      return std::make_unique<FunctionParameters>(std::move(sp), std::move(fps));
    }
    // should have FP
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(TokenType::kComma);
  }
  auto fp1 = parseFunctionParam();
  EXPECT_POINTER_NOT_EMPTY(fp1);
  fps.push_back(std::move(fp1));
  while(!CHECK_TOKEN(TokenType::kRParenthesis)) {
    bool has_comma = CHECK_TOKEN(TokenType::kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(TokenType::kRParenthesis)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(TokenType::kComma);
    auto fp = parseFunctionParam();
    EXPECT_POINTER_NOT_EMPTY(fp);
    fps.push_back(std::move(fp));
  }
  if(!sp && fps.empty())
    REPORT_FAILURE_AND_RETURN("FunctionParameters: expected some params, but got nothing.");
  tracker.commit();
  return std::make_unique<FunctionParameters>(std::move(sp), std::move(fps));
}

std::unique_ptr<FunctionParam> Parser::parseFunctionParam() {
  Backtracker tracker(*_ast_ctx);
  if(auto fpp = parseFunctionParamPattern()) {
    tracker.commit();
    return fpp;
  }
  if(auto fpt = parseFunctionParamType()) {
    tracker.commit();
    return fpt;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<FunctionParamPattern> Parser::parseFunctionParamPattern() {
  Backtracker tracker(*_ast_ctx);
  auto pnta = parsePatternNoTopAlt();
  EXPECT_POINTER_NOT_EMPTY(pnta);
  MATCH_TOKEN(TokenType::kColon);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  tracker.commit();
  return std::make_unique<FunctionParamPattern>(std::move(pnta), std::move(t));
}

std::unique_ptr<FunctionParamType> Parser::parseFunctionParamType() {
  Backtracker tracker(*_ast_ctx);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  tracker.commit();
  return std::make_unique<FunctionParamType>(std::move(t));
}

std::unique_ptr<SelfParam> Parser::parseSelfParam() {
  Backtracker tracker(*_ast_ctx);
  bool is_ref = false;
  if(CHECK_TOKEN(TokenType::kRef)) {
    is_ref = true;
    _ast_ctx->consume();
  }
  bool is_mut = false;
  if(CHECK_TOKEN(TokenType::kMut)) {
    is_mut = true;
    _ast_ctx->consume();
  }
  MATCH_TOKEN(TokenType::kSelfObject);
  std::unique_ptr<Type> t;
  if(CHECK_TOKEN(TokenType::kColon)) {
    _ast_ctx->consume();
    t = parseType();
    EXPECT_POINTER_NOT_EMPTY(t);
  }
  tracker.commit();
  return std::make_unique<SelfParam>(is_ref, is_mut, std::move(t));
}

std::unique_ptr<Type> Parser::parseType() {
  Backtracker tracker(*_ast_ctx);
  if(auto t = parseTypeNoBounds()) {
    tracker.commit();
    return t;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<TypeNoBounds> Parser::parseTypeNoBounds() {
  Backtracker tracker(*_ast_ctx);
  if(auto p = parseParenthesizedType()) {
    tracker.commit();
    return p;
  }
  if(auto t = parseTupleType()) {
    tracker.commit();
    return t;
  }
  if(auto r = parseReferenceType()) {
    tracker.commit();
    return r;
  }
  if(auto a = parseArrayType()) {
    tracker.commit();
    return a;
  }
  if(auto s = parseSliceType()) {
    tracker.commit();
    return s;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<ParenthesizedType> Parser::parseParenthesizedType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kLParenthesis);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(TokenType::kRParenthesis);
  tracker.commit();
  return std::make_unique<ParenthesizedType>(std::move(t));
}

std::unique_ptr<TupleType> Parser::parseTupleType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kLParenthesis);
  std::vector<std::unique_ptr<Type>> ts;
  while(!CHECK_TOKEN(TokenType::kRParenthesis)) {
    auto t = parseType();
    EXPECT_POINTER_NOT_EMPTY(t);
    ts.push_back(std::move(t));
    if(CHECK_TOKEN(TokenType::kComma)) {
      _ast_ctx->consume();
    } else {
      EXPECT_TOKEN(TokenType::kRParenthesis);
      break;
    }
  }
  MATCH_TOKEN(TokenType::kRParenthesis);
  tracker.commit();
  return std::make_unique<TupleType>(std::move(ts));
}

std::unique_ptr<ReferenceType> Parser::parseReferenceType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kAnd);
  bool is_mut = false;
  if(CHECK_TOKEN(TokenType::kMut)) {
    is_mut = true;
    _ast_ctx->consume();
  }
  auto tnb = parseTypeNoBounds();
  EXPECT_POINTER_NOT_EMPTY(tnb);
  tracker.commit();
  return std::make_unique<ReferenceType>(is_mut, std::move(tnb));
}

std::unique_ptr<ArrayType> Parser::parseArrayType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kLSquareBracket);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(TokenType::kSemi);
  auto e = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(e);
  MATCH_TOKEN(TokenType::kRSquareBracket);
  tracker.commit();
  return std::make_unique<ArrayType>(std::move(t), std::move(e));
}

std::unique_ptr<SliceType> Parser::parseSliceType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kLSquareBracket);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(TokenType::kRSquareBracket);
  tracker.commit();
  return std::make_unique<SliceType>(std::move(t));
}

std::unique_ptr<Struct> Parser::parseStruct() {
  Backtracker tracker(*_ast_ctx);
  if(auto ss = parseStructStruct()) {
    tracker.commit();
    return ss;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<StructStruct> Parser::parseStructStruct() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kStruct);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  std::unique_ptr<StructFields> sf_opt;
  if(CHECK_TOKEN(TokenType::kLCurlyBrace)) {
    _ast_ctx->consume();
    sf_opt = parseStructFields();
    EXPECT_POINTER_NOT_EMPTY(sf_opt);
    MATCH_TOKEN(TokenType::kRCurlyBrace);
  } else {
    MATCH_TOKEN(TokenType::kSemi);
  }
  tracker.commit();
  return std::make_unique<StructStruct>(ident, std::move(sf_opt));
}

std::unique_ptr<StructFields> Parser::parseStructFields() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<StructField>> sfs;
  auto sf1 = parseStructField();
  EXPECT_POINTER_NOT_EMPTY(sf1);
  sfs.push_back(std::move(sf1));
  while(!CHECK_TOKEN(TokenType::kRCurlyBrace)) {
    bool has_comma = CHECK_TOKEN(TokenType::kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(TokenType::kRCurlyBrace)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(TokenType::kComma);
    auto sf = parseStructField();
    EXPECT_POINTER_NOT_EMPTY(sf);
    sfs.push_back(std::move(sf));
  }
  tracker.commit();
  return std::make_unique<StructFields>(std::move(sfs));
}

std::unique_ptr<StructField> Parser::parseStructField() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(TokenType::kColon);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  tracker.commit();
  return std::make_unique<StructField>(ident, std::move(t));
}

std::unique_ptr<Enumeration> Parser::parseEnumeration() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kEnum);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(TokenType::kLCurlyBrace);
  auto ei_opt = parseEnumItems();
  MATCH_TOKEN(TokenType::kRCurlyBrace);
  tracker.commit();
  return std::make_unique<Enumeration>(ident, std::move(ei_opt));
}

std::unique_ptr<EnumItems> Parser::parseEnumItems() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<EnumItem>> eis;
  auto ei1 = parseEnumItem();
  EXPECT_POINTER_NOT_EMPTY(ei1);
  eis.push_back(std::move(ei1));
  while(!CHECK_TOKEN(TokenType::kRCurlyBrace)) {
    bool has_comma = CHECK_TOKEN(TokenType::kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(TokenType::kRCurlyBrace)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(TokenType::kComma);
    auto ei = parseEnumItem();
    EXPECT_POINTER_NOT_EMPTY(ei);
    eis.push_back(std::move(ei));
  }
  tracker.commit();
  return std::make_unique<EnumItems>(std::move(eis));
}

std::unique_ptr<EnumItem> Parser::parseEnumItem() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  auto eid_opt = parseEnumItemDiscriminant();
  tracker.commit();
  return std::make_unique<EnumItem>(ident, std::move(eid_opt));
}

std::unique_ptr<EnumItemDiscriminant> Parser::parseEnumItemDiscriminant() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kEq);
  auto e = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(e);
  tracker.commit();
  return std::make_unique<EnumItemDiscriminant>(std::move(e));
}

std::unique_ptr<ConstantItem> Parser::parseConstantItem() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kConst);
  std::string_view ident;
  if(CHECK_TOKEN(TokenType::kIdentifier) || CHECK_TOKEN(TokenType::kUnderscore)) {
    ident = _ast_ctx->current().lexeme;
    _ast_ctx->consume();
  } else {
    REPORT_FAILURE_AND_RETURN("ConstantItem: expected identifier or underscore.");
  }
  MATCH_TOKEN(TokenType::kColon);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  std::unique_ptr<Expression> e_opt;
  if(CHECK_TOKEN(TokenType::kEq)) {
    _ast_ctx->consume();
    e_opt = parseExpression();
    EXPECT_POINTER_NOT_EMPTY(e_opt);
  }
  MATCH_TOKEN(TokenType::kSemi);
  tracker.commit();
  return std::make_unique<ConstantItem>(ident, std::move(t), std::move(e_opt));
}

std::unique_ptr<Trait> Parser::parseTrait() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kTrait);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(TokenType::kLCurlyBrace);
  std::vector<std::unique_ptr<AssociatedItem>> ais;
  while(!CHECK_TOKEN(TokenType::kRCurlyBrace)) {
    auto ai = parseAssociatedItem();
    EXPECT_POINTER_NOT_EMPTY(ai);
    ais.push_back(std::move(ai));
  }
  MATCH_TOKEN(TokenType::kRCurlyBrace);
  tracker.commit();
  return std::make_unique<Trait>(ident, std::move(ais));
}

std::unique_ptr<AssociatedItem> Parser::parseAssociatedItem() {
  Backtracker tracker(*_ast_ctx);
  if(auto a = parseAssociatedTypeAlias()) {
    tracker.commit();
    return a;
  }
  if(auto a = parseAssociatedConstantItem()) {
    tracker.commit();
    return a;
  }
  if(auto a = parseAssociatedFunction()) {
    tracker.commit();
    return a;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<AssociatedTypeAlias> Parser::parseAssociatedTypeAlias() {
  Backtracker tracker(*_ast_ctx);
  auto t = parseTypeAlias();
  EXPECT_POINTER_NOT_EMPTY(t);
  tracker.commit();
  return std::make_unique<AssociatedTypeAlias>(std::move(t));
}

std::unique_ptr<AssociatedConstantItem> Parser::parseAssociatedConstantItem() {
  Backtracker tracker(*_ast_ctx);
  auto c = parseConstantItem();
  EXPECT_POINTER_NOT_EMPTY(c);
  tracker.commit();
  return std::make_unique<AssociatedConstantItem>(std::move(c));
}

std::unique_ptr<AssociatedFunction> Parser::parseAssociatedFunction() {
  Backtracker tracker(*_ast_ctx);
  auto f = parseFunction();
  EXPECT_POINTER_NOT_EMPTY(f);
  tracker.commit();
  return std::make_unique<AssociatedFunction>(std::move(f));
}

std::unique_ptr<TypeAlias> Parser::parseTypeAlias() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kType);
  EXPECT_TOKEN(TokenType::kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  std::unique_ptr<Type> t_opt;
  if(CHECK_TOKEN(TokenType::kEq)) {
    _ast_ctx->consume();
    t_opt = parseType();
    EXPECT_POINTER_NOT_EMPTY(t_opt);
  }
  MATCH_TOKEN(TokenType::kSemi);
  tracker.commit();
  return std::make_unique<TypeAlias>(ident, std::move(t_opt));
}

std::unique_ptr<Implementation> Parser::parseImplementation() {
  Backtracker tracker(*_ast_ctx);
  if(auto i = parseInherentImpl()) {
    tracker.commit();
    return i;
  }
  if(auto t = parseTraitImpl()) {
    tracker.commit();
    return t;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<InherentImpl> Parser::parseInherentImpl() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kImpl);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(TokenType::kLCurlyBrace);
  std::vector<std::unique_ptr<AssociatedItem>> ais;
  while(!CHECK_TOKEN(TokenType::kRCurlyBrace)) {
    auto ai = parseAssociatedItem();
    EXPECT_POINTER_NOT_EMPTY(ai);
    ais.push_back(std::move(ai));
  }
  MATCH_TOKEN(TokenType::kRCurlyBrace);
  tracker.commit();
  return std::make_unique<InherentImpl>(std::move(t), std::move(ais));
}

std::unique_ptr<TraitImpl> Parser::parseTraitImpl() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(TokenType::kImpl);
  auto tp = parseTypePath();
  EXPECT_POINTER_NOT_EMPTY(tp);
  MATCH_TOKEN(TokenType::kFor);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(TokenType::kLCurlyBrace);
  std::vector<std::unique_ptr<AssociatedItem>> ais;
  while(!CHECK_TOKEN(TokenType::kRCurlyBrace)) {
    auto ai = parseAssociatedItem();
    EXPECT_POINTER_NOT_EMPTY(ai);
    ais.push_back(std::move(ai));
  }
  MATCH_TOKEN(TokenType::kRCurlyBrace);
  tracker.commit();
  return std::make_unique<TraitImpl>(std::move(tp), std::move(t), std::move(ais));
}

std::unique_ptr<TypePath> Parser::parseTypePath() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<TypePathSegment>> tpss;
  auto tps1 = parseTypePathSegment();
  EXPECT_POINTER_NOT_EMPTY(tps1);
  tpss.push_back(std::move(tps1));
  while(CHECK_TOKEN(TokenType::kPathSep)) {
    _ast_ctx->consume();
    auto tps = parseTypePathSegment();
    EXPECT_POINTER_NOT_EMPTY(tps);
    tpss.push_back(std::move(tps));
  }
  tracker.commit();
  return std::make_unique<TypePath>(std::move(tpss));
}

std::unique_ptr<TypePathSegment> Parser::parseTypePathSegment() {
  Backtracker tracker(*_ast_ctx);
  auto pid = parsePathIdentSegment();
  EXPECT_POINTER_NOT_EMPTY(pid);
  tracker.commit();
  return std::make_unique<TypePathSegment>(std::move(pid));
}

std::unique_ptr<PathIdentSegment> Parser::parsePathIdentSegment() {
  Backtracker tracker(*_ast_ctx);
  std::string_view ident;
  if(CHECK_TOKEN(TokenType::kIdentifier) || CHECK_TOKEN(TokenType::kSuper) || CHECK_TOKEN(TokenType::kSelfObject) ||
    CHECK_TOKEN(TokenType::kSelfType) || CHECK_TOKEN(TokenType::kCrate)) {
    ident = _ast_ctx->current().lexeme;
    _ast_ctx->consume();
  } else {
    REPORT_FAILURE_AND_RETURN("PathIdentSegment: Not an identifier or an allowed keyword.");
  }
  tracker.commit();
  return std::make_unique<PathIdentSegment>(ident);
}

enum class Precedence {
  kLowest = 0,          // termination & default defence
  kAssignment = 1,      // =, +=, -=, etc.
  kTypeCast = 2,        // as
  kRange = 3,           // .., ..=
  kLogicalOr = 4,       // ||
  kLogicalAnd = 5,      // &&
  kComparison = 6,      // ==, !=, >, <, >=, <=
  kBitwise = 7,         // &, |, ^
  kShift = 8,           // <<, >>
  kAdditive = 9,        // +, -
  kMultiplicative = 10, // *, /, %
  kPrefix = 11,         // -, !, *, &, &&
  kCallAndMember = 12,  // (), [], ., ::
  kHighest = 13,        // defence
};

std::map<TokenType, int> prefix_precedence = {
  // Unary prefix operators' precedence
  {TokenType::kMinus,     static_cast<int>(Precedence::kPrefix)},
  {TokenType::kNot,       static_cast<int>(Precedence::kPrefix)},
  {TokenType::kStar,      static_cast<int>(Precedence::kPrefix)}, // Dereference
  {TokenType::kAnd,       static_cast<int>(Precedence::kPrefix)}, // Borrow `&`
  {TokenType::kAndAnd,    static_cast<int>(Precedence::kPrefix)}, // Borrow `&&`

  // These keywords act as expression starters
  {TokenType::kReturn,    static_cast<int>(Precedence::kLowest)},
  {TokenType::kBreak,     static_cast<int>(Precedence::kLowest)},
  {TokenType::kContinue,  static_cast<int>(Precedence::kLowest)},
  {TokenType::kUnderscore,static_cast<int>(Precedence::kLowest)},
};

std::map<TokenType, int> infix_precedence = {
    // Member access and function calls
    {TokenType::kLParenthesis,     static_cast<int>(Precedence::kCallAndMember)}, // ()
    {TokenType::kLSquareBracket,   static_cast<int>(Precedence::kCallAndMember)}, // []
    {TokenType::kDot,              static_cast<int>(Precedence::kCallAndMember)}, // .
    {TokenType::kPathSep,          static_cast<int>(Precedence::kCallAndMember)}, // ::

    // Multiplicative
    {TokenType::kStar,        static_cast<int>(Precedence::kMultiplicative)},
    {TokenType::kSlash,       static_cast<int>(Precedence::kMultiplicative)},
    {TokenType::kPercent,     static_cast<int>(Precedence::kMultiplicative)},

    // Additive
    {TokenType::kPlus,        static_cast<int>(Precedence::kAdditive)},
    {TokenType::kMinus,       static_cast<int>(Precedence::kAdditive)},

    // Shift
    {TokenType::kShl,         static_cast<int>(Precedence::kShift)},
    {TokenType::kShr,         static_cast<int>(Precedence::kShift)},

    // Bitwise
    {TokenType::kAnd,         static_cast<int>(Precedence::kBitwise)},
    {TokenType::kOr,          static_cast<int>(Precedence::kBitwise)},
    {TokenType::kCaret,       static_cast<int>(Precedence::kBitwise)},

    // Comparison
    {TokenType::kEqEq,        static_cast<int>(Precedence::kComparison)},
    {TokenType::kNe,          static_cast<int>(Precedence::kComparison)},
    {TokenType::kGt,          static_cast<int>(Precedence::kComparison)},
    {TokenType::kLt,          static_cast<int>(Precedence::kComparison)},
    {TokenType::kGe,          static_cast<int>(Precedence::kComparison)},
    {TokenType::kLe,          static_cast<int>(Precedence::kComparison)},

    // Logical
    {TokenType::kAndAnd,      static_cast<int>(Precedence::kLogicalAnd)},
    {TokenType::kOrOr,        static_cast<int>(Precedence::kLogicalOr)},

    // Type casting
    {TokenType::kAs,          static_cast<int>(Precedence::kTypeCast)},

    // Range
    {TokenType::kDotDot,      static_cast<int>(Precedence::kRange)},
    {TokenType::kDotDotDot,   static_cast<int>(Precedence::kRange)},
    {TokenType::kDotDotEq,    static_cast<int>(Precedence::kRange)},

    // Assignment
    {TokenType::kEq,          static_cast<int>(Precedence::kAssignment)},
    {TokenType::kPlusEq,      static_cast<int>(Precedence::kAssignment)},
    {TokenType::kMinusEq,     static_cast<int>(Precedence::kAssignment)},
    {TokenType::kStarEq,      static_cast<int>(Precedence::kAssignment)},
    {TokenType::kSlashEq,     static_cast<int>(Precedence::kAssignment)},
    {TokenType::kPercentEq,   static_cast<int>(Precedence::kAssignment)},
    {TokenType::kCaretEq,     static_cast<int>(Precedence::kAssignment)},
    {TokenType::kAndEq,       static_cast<int>(Precedence::kAssignment)},
    {TokenType::kOrEq,        static_cast<int>(Precedence::kAssignment)},
    {TokenType::kShlEq,       static_cast<int>(Precedence::kAssignment)},
    {TokenType::kShrEq,       static_cast<int>(Precedence::kAssignment)},
};

Operator token_to_operator(TokenType type) {
  static const std::unordered_map<TokenType, Operator> fmap = {
    // Arithmetic Operators
    {TokenType::kPlus,    Operator::kAdd},
    {TokenType::kMinus,   Operator::kSub},
    {TokenType::kStar,    Operator::kMul},
    {TokenType::kSlash,   Operator::kDiv},
    {TokenType::kPercent, Operator::kMod},
    {TokenType::kCaret,   Operator::kPow},

    // Compound Assignment Operators
    {TokenType::kPlusEq,   Operator::kAddAssign},
    {TokenType::kMinusEq,  Operator::kSubAssign},
    {TokenType::kStarEq,   Operator::kMulAssign},
    {TokenType::kSlashEq,  Operator::kDivAssign},
    {TokenType::kPercentEq,Operator::kModAssign},
    {TokenType::kCaretEq,  Operator::kPowAssign},

    // Logical and Comparison Operators
    {TokenType::kAnd,      Operator::kAnd},
    {TokenType::kOr,       Operator::kOr},
    {TokenType::kNot,      Operator::kNot},
    {TokenType::kAndAnd,   Operator::kLogicalAnd},
    {TokenType::kOrOr,     Operator::kLogicalOr},
    {TokenType::kEqEq,     Operator::kEq},
    {TokenType::kNe,       Operator::kNe},
    {TokenType::kGt,       Operator::kGt},
    {TokenType::kLt,       Operator::kLt},
    {TokenType::kGe,       Operator::kGe},
    {TokenType::kLe,       Operator::kLe},

    // Bitwise Operators
    // Note: kAnd, kOr, and kCaret have dual use with logical/arithmetic.
    // The parser's context (e.g., in a bitwise expression context)
    // determines the specific operator. This map provides the general
    // mapping. The specific bitwise assignments are unambiguous.
    {TokenType::kShl,      Operator::kShl},
    {TokenType::kShr,      Operator::kShr},
    {TokenType::kAndEq,    Operator::kBitwiseAndAssign},
    {TokenType::kOrEq,     Operator::kBitwiseOrAssign},
    {TokenType::kShlEq,    Operator::kShlAssign},
    {TokenType::kShrEq,    Operator::kShrAssign},

    // Special Assignment
    {TokenType::kEq,       Operator::kAssign},

    // Pointer/Dereference
    // kStar and kAnd are context-dependent for their pointer meaning.
    // The parser must determine if they are kDeref/kRef or kMul/kBitwiseAnd
    // based on their position (prefix vs. infix).
};

  if (auto it = fmap.find(type); it != fmap.end()) {
    return it->second;
  }
  return Operator::kInvalid;
}

std::unique_ptr<Expression> Parser::parsePrattExpression(int precedence) {
  auto lft = parsePrefixExpression();
  EXPECT_POINTER_NOT_EMPTY(lft);
  while(infix_precedence.contains(_ast_ctx->current().token_type) &&
    infix_precedence[_ast_ctx->current().token_type] > precedence) {
    auto type = _ast_ctx->current().token_type;
    int new_pred = infix_precedence[type];
    _ast_ctx->consume();
    switch(type) {
      // prefix: field, func call, index field...?
    case TokenType::kDot: {
      lft = parseFieldExpression(std::move(lft));
    } break;
    case TokenType::kLParenthesis: {
      lft = parseCallExpression(std::move(lft));
    } break;
    case TokenType::kLSquareBracket: {
      lft = parseIndexExpression(std::move(lft));
    } break;
      // infix
    case TokenType::kPlus: case TokenType::kMinus: case TokenType::kStar:
    case TokenType::kSlash: case TokenType::kPercent: case TokenType::kCaret:
    case TokenType::kAnd: case TokenType::kOr: case TokenType::kShl:
    case TokenType::kShr: {
      auto rht = parsePrattExpression(new_pred);
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<ArithmeticOrLogicalExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kEqEq: case TokenType::kNe: case TokenType::kGt:
    case TokenType::kLt: case TokenType::kGe: case TokenType::kLe: {
      auto rht = parsePrattExpression(new_pred);
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<ComparisonExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kAndAnd: case TokenType::kOrOr: {
      auto rht = parsePrattExpression(new_pred);
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<LazyBooleanExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kEq: {
      auto rht = parsePrattExpression(new_pred - 1); // right associative
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<AssignmentExpression>(std::move(lft), std::move(rht));
    } break;
    case TokenType::kPlusEq: case TokenType::kMinusEq: case TokenType::kStarEq:
    case TokenType::kSlashEq: case TokenType::kPercentEq: case TokenType::kCaretEq:
    case TokenType::kAndEq: case TokenType::kOrEq: case TokenType::kShlEq:
    case TokenType::kShrEq: {
      auto rht = parsePrattExpression(new_pred - 1); // right associative
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<CompoundAssignmentExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kAs: {
      auto tnb = parseTypeNoBounds();
      EXPECT_POINTER_NOT_EMPTY(tnb);
      lft = std::make_unique<TypeCastExpression>(std::move(lft), std::move(tnb));
    } break;
    case TokenType::kDotDot: case TokenType::kDotDotDot: case TokenType::kDotDotEq: {
      lft = parseRangeExpression(std::move(lft), token_to_operator(type));
    } break;
    default:
      REPORT_FAILURE_AND_RETURN(
        "Unexpected infix operator token: " + token_type_to_string(type)
      );
    }
  }
  EXPECT_POINTER_NOT_EMPTY(lft);
  return lft;
}

std::unique_ptr<Expression> Parser::parsePrefixExpression() {
  EXPECT_CONTEXT_NOT_EMPTY();
  auto ctt = _ast_ctx->current().token_type; // current token type
  auto ctc = get_token_category(ctt); // current token type category
  if(ctc == TokenTypeCat::kLiteral || ctt == TokenType::kTrue || ctt == TokenType::kFalse) {
    _ast_ctx->consume();
    return parseLiteralExpression();
  }
  if(ctc == TokenTypeCat::kIdentifier) {
    return parsePathExpression();
  }
  switch(ctt) {
    // unary
  case TokenType::kMinus: case TokenType::kNot: {
    _ast_ctx->consume();
    auto expr = parsePrattExpression(static_cast<int>(Precedence::kPrefix));
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<NegationExpression>(token_to_operator(ctt), std::move(expr));
  }
  case TokenType::kStar: {
    _ast_ctx->consume();
    auto expr = parsePrattExpression(static_cast<int>(Precedence::kPrefix));
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<DereferenceExpression>(std::move(expr));
  }
  case TokenType::kAnd: {
    _ast_ctx->consume();
    bool is_mut = false;
    if(CHECK_TOKEN(kMut)) {
      is_mut = true;
      _ast_ctx->consume();
    }
    auto expr = parsePrattExpression(static_cast<int>(Precedence::kPrefix));
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<BorrowExpression>(is_mut, std::move(expr));
  }
  case TokenType::kAndAnd: {
    _ast_ctx->consume(); // "&&" is one token
    bool is_mut = false;
    if(CHECK_TOKEN(kMut)) {
      is_mut = true;
      _ast_ctx->consume();
    }
    auto expr = parsePrattExpression(static_cast<int>(Precedence::kPrefix));
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<BorrowExpression>(false, std::make_unique<BorrowExpression>(is_mut, std::move(expr)));
  }
    // keywords and punctuations
  case TokenType::kUnderscore: return parseUnderscoreExpression();
  case TokenType::kBreak: return parseBreakExpression();
  case TokenType::kContinue: return parseContinueExpression();
  case TokenType::kReturn: return parseReturnExpression();
  case TokenType::kIf: return parseIfExpression();
  case TokenType::kLoop: return parseLoopExpression();
  case TokenType::kWhile: return parsePredicateLoopExpression();
  case TokenType::kMatch: return parseMatchExpression();
  case TokenType::kLCurlyBrace: return parseBlockExpression();
  case TokenType::kLParenthesis: {
    if(_ast_ctx->peek(1).token_type == TokenType::kRParenthesis) {
      return parseTupleExpression(); // unit type "()"
    }
    auto expr = parsePrattExpression(static_cast<int>(Precedence::kLowest));
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<GroupedExpression>(std::move(expr));
  }
  case TokenType::kLSquareBracket: return parseArrayExpression();
  default: REPORT_FAILURE_AND_RETURN("Unexpected token in expression:" + token_type_to_string(ctt));
  }
  REPORT_FAILURE_AND_RETURN("Unexpected token in expression:" + token_type_to_string(ctt));
}

std::unique_ptr<Expression> Parser::parseExpression() {
  Backtracker tracker(*_ast_ctx);
  auto e = parsePrattExpression(static_cast<int>(Precedence::kLowest));
  EXPECT_POINTER_NOT_EMPTY(e);
  tracker.commit();
  return e;
}

std::unique_ptr<ExpressionWithoutBlock> Parser::parseExpressionWithoutBlock() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<LiteralExpression> Parser::parseLiteralExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathExpression> Parser::parsePathExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathInExpression> Parser::parsePathInExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathExprSegment> Parser::parsePathExprSegment() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<OperatorExpression> Parser::parseOperatorExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<BorrowExpression> Parser::parseBorrowExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<DereferenceExpression> Parser::parseDereferenceExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<NegationExpression> Parser::parseNegationExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ArithmeticOrLogicalExpression> Parser::parseArithmeticOrLogicalExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ComparisonExpression> Parser::parseComparisonExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<LazyBooleanExpression> Parser::parseLazyBooleanExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<TypeCastExpression> Parser::parseTypeCastExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<AssignmentExpression> Parser::parseAssignmentExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<CompoundAssignmentExpression> Parser::parseCompoundAssignmentExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<GroupedExpression> Parser::parseGroupedExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ArrayExpression> Parser::parseArrayExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ArrayElements> Parser::parseArrayElements() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExplicitArrayElements> Parser::parseExplicitArrayElements() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RepeatedArrayElements> Parser::parseRepeatedArrayElements() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<IndexExpression> Parser::parseIndexExpression(std::unique_ptr<Expression> &&lft) {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<TupleExpression> Parser::parseTupleExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<TupleElements> Parser::parseTupleElements() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<TupleIndexingExpression> Parser::parseTupleIndexingExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructExpression> Parser::parseStructExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructExprFields> Parser::parseStructExprFields() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructExprField> Parser::parseStructExprField() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<NamedStructExprField> Parser::parseNamedStructExprField() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<IndexStructExprField> Parser::parseIndexStructExprField() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<CallExpression> Parser::parseCallExpression(std::unique_ptr<Expression> &&lft) {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<CallParams> Parser::parseCallParams() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<MethodCallExpression> Parser::parseMethodCallExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<FieldExpression> Parser::parseFieldExpression(std::unique_ptr<Expression> &&lft) {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ContinueExpression> Parser::parseContinueExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<BreakExpression> Parser::parseBreakExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeExpression> Parser::parseRangeExpression(std::unique_ptr<Expression> &&expr, Operator oper) {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeExpr> Parser::parseRangeExpr() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeFromExpr> Parser::parseRangeFromExpr() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeToExpr> Parser::parseRangeToExpr() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeFullExpr> Parser::parseRangeFullExpr() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeInclusiveExpr> Parser::parseRangeInclusiveExpr() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<RangeToInclusiveExpr> Parser::parseRangeToInclusiveExpr() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ReturnExpression> Parser::parseReturnExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<UnderscoreExpression> Parser::parseUnderscoreExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExpressionWithBlock> Parser::parseExpressionWithBlock() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<BlockExpression> Parser::parseBlockExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<Statements> Parser::parseStatements() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<Statement> Parser::parseStatement() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<EmptyStatement> Parser::parseEmptyStatement() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ItemStatement> Parser::parseItemStatement() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<LetStatement> Parser::parseLetStatement() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ExpressionStatement> Parser::parseExpressionStatement() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<LoopExpression> Parser::parseLoopExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<InfiniteLoopExpression> Parser::parseInfiniteLoopExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PredicateLoopExpression> Parser::parsePredicateLoopExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<IfExpression> Parser::parseIfExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<Conditions> Parser::parseConditions() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchExpression> Parser::parseMatchExpression() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchArms> Parser::parseMatchArms() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchArm> Parser::parseMatchArm() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<MatchArmGuard> Parser::parseMatchArmGuard() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<Pattern> Parser::parsePattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PatternNoTopAlt> Parser::parsePatternNoTopAlt() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PatternWithoutRange> Parser::parsePatternWithoutRange() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<LiteralPattern> Parser::parseLiteralPattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<IdentifierPattern> Parser::parseIdentifierPattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<WildcardPattern> Parser::parseWildcardPattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<ReferencePattern> Parser::parseReferencePattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPattern> Parser::parseStructPattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPatternElements> Parser::parseStructPatternElements() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPatternFields> Parser::parseStructPatternFields() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<StructPatternField> Parser::parseStructPatternField() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<TuplePattern> Parser::parseTuplePattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<TuplePatternItems> Parser::parseTuplePatternItems() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<GroupedPattern> Parser::parseGroupedPattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<SlicePattern> Parser::parseSlicePattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<SlicePatternItems> Parser::parseSlicePatternItems() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

std::unique_ptr<PathPattern> Parser::parsePathPattern() {
  Backtracker tracker(*_ast_ctx);

  tracker.commit();
  return nullptr;
}

}

#undef REPORT_FAILURE_COMMON
#undef REPORT_PARSE_FAILURE_AND_RETURN
#undef REPORT_MISSING_TOKEN_AND_RETURN
#undef EXPECT_CONTEXT_NOT_EMPTY
#undef EXPECT_POINTER_NOT_EMPTY
#undef CHECK_TOKEN
#undef EXPECT_TOKEN
#undef MATCH_TOKEN