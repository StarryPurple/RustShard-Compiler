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
      "Unexpected parse failure." \
    ); \
  } while(false)

#define REPORT_MISSING_TOKEN_AND_RETURN(ExpectedType) \
  do { \
    REPORT_FAILURE_AND_RETURN( \
      " Unexpected parse failure: Expected puntuation " + \
      token_type_to_string(TokenType::ExpectedType) + " not appearing." \
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
        "Unexpected parse failure: Unrecognized possibility." \
      ); \
    } \
  } while(false)

#define CHECK_TOKEN(ExpectedType) \
  (_ast_ctx->current().type == TokenType::ExpectedType)

#define EXPECT_TOKEN(ExpectedType) \
  do { \
    if(!CHECK_TOKEN(ExpectedType)) { \
      REPORT_FAILURE_AND_RETURN( \
        "Unexpected token at" + \
        " row:" + std::to_string(_ast_ctx->current().row) + \
        " col:" + std::to_string(_ast_ctx->current().col) + \
        ". Expected " + #ExpectedType + \
        ", but got " + token_type_to_string(_ast_ctx->current().type) \
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



class Parser::Backtracker {
  Context &_ast_ctx;
  std::size_t _pos, _err_pos;
  bool _commited;
public:
  explicit Backtracker(Context &ast_ctx)
  : _ast_ctx(ast_ctx), _pos(ast_ctx._pos),
  _err_pos(ast_ctx._errors.size()), _commited(false) {}
  ~Backtracker() {
    if(!_commited) {
      _ast_ctx._pos = _pos;
    } else {
      _ast_ctx._errors.resize(_err_pos);
    }
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

void Parser::parseAll(const Lexer &lexer) {
  _ast_ctx = std::make_unique<Context>(lexer.tokens());
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
  if(CHECK_TOKEN(kConst)) {
    is_const = true;
    _ast_ctx->consume();
  }
  MATCH_TOKEN(kFn);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(kLParenthesis);
  auto fp_opt = parseFunctionParameters();
  MATCH_TOKEN(kRParenthesis);
  std::unique_ptr<Type> t_opt;
  if(CHECK_TOKEN(kRArrow)) {
    _ast_ctx->consume();
    t_opt = parseType();
    EXPECT_POINTER_NOT_EMPTY(t_opt);
  }
  std::unique_ptr<BlockExpression> be_opt;
  if(CHECK_TOKEN(kSemi)) {
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
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRParenthesis)) {
      // SP ','? {')'}
      tracker.commit();
      return std::make_unique<FunctionParameters>(std::move(sp), std::move(fps));
    }
    // should have FP
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
  }
  auto fp1 = parseFunctionParam();
  EXPECT_POINTER_NOT_EMPTY(fp1);
  fps.push_back(std::move(fp1));
  while(!CHECK_TOKEN(kRParenthesis)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRParenthesis)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
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
  MATCH_TOKEN(kColon);
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
  if(CHECK_TOKEN(kRef)) {
    is_ref = true;
    _ast_ctx->consume();
  }
  bool is_mut = false;
  if(CHECK_TOKEN(kMut)) {
    is_mut = true;
    _ast_ctx->consume();
  }
  MATCH_TOKEN(kSelfObject);
  std::unique_ptr<Type> t;
  if(CHECK_TOKEN(kColon)) {
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
  if(auto t = parseTypePath()) {
    tracker.commit();
    return t;
  }
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
  MATCH_TOKEN(kLParenthesis);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(kRParenthesis);
  tracker.commit();
  return std::make_unique<ParenthesizedType>(std::move(t));
}

std::unique_ptr<TupleType> Parser::parseTupleType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLParenthesis);
  std::vector<std::unique_ptr<Type>> ts;
  while(!CHECK_TOKEN(kRParenthesis)) {
    auto t = parseType();
    EXPECT_POINTER_NOT_EMPTY(t);
    ts.push_back(std::move(t));
    if(CHECK_TOKEN(kComma)) {
      _ast_ctx->consume();
    } else {
      EXPECT_TOKEN(kRParenthesis);
      break;
    }
  }
  MATCH_TOKEN(kRParenthesis);
  tracker.commit();
  return std::make_unique<TupleType>(std::move(ts));
}

std::unique_ptr<ReferenceType> Parser::parseReferenceType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kAnd);
  bool is_mut = false;
  if(CHECK_TOKEN(kMut)) {
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
  MATCH_TOKEN(kLSquareBracket);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(kSemi);
  auto e = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(e);
  MATCH_TOKEN(kRSquareBracket);
  tracker.commit();
  return std::make_unique<ArrayType>(std::move(t), std::move(e));
}

std::unique_ptr<SliceType> Parser::parseSliceType() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLSquareBracket);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(kRSquareBracket);
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
  MATCH_TOKEN(kStruct);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  std::unique_ptr<StructFields> sf_opt;
  if(CHECK_TOKEN(kLCurlyBrace)) {
    _ast_ctx->consume();
    sf_opt = parseStructFields();
    EXPECT_POINTER_NOT_EMPTY(sf_opt);
    MATCH_TOKEN(kRCurlyBrace);
  } else {
    MATCH_TOKEN(kSemi);
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
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRCurlyBrace)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto sf = parseStructField();
    EXPECT_POINTER_NOT_EMPTY(sf);
    sfs.push_back(std::move(sf));
  }
  tracker.commit();
  return std::make_unique<StructFields>(std::move(sfs));
}

std::unique_ptr<StructField> Parser::parseStructField() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(kColon);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  tracker.commit();
  return std::make_unique<StructField>(ident, std::move(t));
}

std::unique_ptr<Enumeration> Parser::parseEnumeration() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kEnum);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(kLCurlyBrace);
  auto ei_opt = parseEnumItems();
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<Enumeration>(ident, std::move(ei_opt));
}

std::unique_ptr<EnumItems> Parser::parseEnumItems() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<EnumItem>> eis;
  auto ei1 = parseEnumItem();
  EXPECT_POINTER_NOT_EMPTY(ei1);
  eis.push_back(std::move(ei1));
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRCurlyBrace)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto ei = parseEnumItem();
    EXPECT_POINTER_NOT_EMPTY(ei);
    eis.push_back(std::move(ei));
  }
  tracker.commit();
  return std::make_unique<EnumItems>(std::move(eis));
}

std::unique_ptr<EnumItem> Parser::parseEnumItem() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  auto eid_opt = parseEnumItemDiscriminant();
  tracker.commit();
  return std::make_unique<EnumItem>(ident, std::move(eid_opt));
}

std::unique_ptr<EnumItemDiscriminant> Parser::parseEnumItemDiscriminant() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kEq);
  auto e = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(e);
  tracker.commit();
  return std::make_unique<EnumItemDiscriminant>(std::move(e));
}

std::unique_ptr<ConstantItem> Parser::parseConstantItem() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kConst);
  std::string_view ident;
  if(CHECK_TOKEN(kIdentifier) || CHECK_TOKEN(kUnderscore)) {
    ident = _ast_ctx->current().lexeme;
    _ast_ctx->consume();
  } else {
    REPORT_FAILURE_AND_RETURN("ConstantItem: expected identifier or underscore.");
  }
  MATCH_TOKEN(kColon);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  std::unique_ptr<Expression> e_opt;
  if(CHECK_TOKEN(kEq)) {
    _ast_ctx->consume();
    e_opt = parseExpression();
    EXPECT_POINTER_NOT_EMPTY(e_opt);
  }
  MATCH_TOKEN(kSemi);
  tracker.commit();
  return std::make_unique<ConstantItem>(ident, std::move(t), std::move(e_opt));
}

std::unique_ptr<Trait> Parser::parseTrait() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kTrait);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(kLCurlyBrace);
  std::vector<std::unique_ptr<AssociatedItem>> ais;
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    auto ai = parseAssociatedItem();
    EXPECT_POINTER_NOT_EMPTY(ai);
    ais.push_back(std::move(ai));
  }
  MATCH_TOKEN(kRCurlyBrace);
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
  MATCH_TOKEN(kType);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  std::unique_ptr<Type> t_opt;
  if(CHECK_TOKEN(kEq)) {
    _ast_ctx->consume();
    t_opt = parseType();
    EXPECT_POINTER_NOT_EMPTY(t_opt);
  }
  MATCH_TOKEN(kSemi);
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
  MATCH_TOKEN(kImpl);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(kLCurlyBrace);
  std::vector<std::unique_ptr<AssociatedItem>> ais;
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    auto ai = parseAssociatedItem();
    EXPECT_POINTER_NOT_EMPTY(ai);
    ais.push_back(std::move(ai));
  }
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<InherentImpl>(std::move(t), std::move(ais));
}

std::unique_ptr<TraitImpl> Parser::parseTraitImpl() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kImpl);
  auto tp = parseTypePath();
  EXPECT_POINTER_NOT_EMPTY(tp);
  MATCH_TOKEN(kFor);
  auto t = parseType();
  EXPECT_POINTER_NOT_EMPTY(t);
  MATCH_TOKEN(kLCurlyBrace);
  std::vector<std::unique_ptr<AssociatedItem>> ais;
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    auto ai = parseAssociatedItem();
    EXPECT_POINTER_NOT_EMPTY(ai);
    ais.push_back(std::move(ai));
  }
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<TraitImpl>(std::move(tp), std::move(t), std::move(ais));
}

std::unique_ptr<TypePath> Parser::parseTypePath() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<TypePathSegment>> tpss;
  auto tps1 = parseTypePathSegment();
  EXPECT_POINTER_NOT_EMPTY(tps1);
  tpss.push_back(std::move(tps1));
  while(CHECK_TOKEN(kPathSep)) {
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
  if(CHECK_TOKEN(kIdentifier) || CHECK_TOKEN(kSuper) || CHECK_TOKEN(kSelfObject) ||
    CHECK_TOKEN(kSelfType) || CHECK_TOKEN(kCrate)) {
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

std::string remove_underscore(std::string_view str) {
  static std::string res; res.clear();
  for(const char c: str) if(c != '_') res += c;
  return res;
}

std::uint64_t resolve_integer(std::string_view lexeme) {
  std::size_t first_alpha = lexeme.find_first_of("iu");
  std::string value_str = remove_underscore(lexeme.substr(0, first_alpha));
  return std::stoull(value_str);
}

std::unique_ptr<Expression> Parser::parseInfixExpression(int precedence) {
  auto lft = parsePrefixExpression();
  EXPECT_POINTER_NOT_EMPTY(lft);
  while(infix_precedence.contains(_ast_ctx->current().type) &&
    infix_precedence[_ast_ctx->current().type] > precedence) {
    auto type = _ast_ctx->current().type;
    int new_pred = infix_precedence[type];
    switch(type) {
      // prefix: field, func call, index field...?
    case TokenType::kDot: {
      _ast_ctx->consume();
      if(CHECK_TOKEN(kIntegerLiteral)) {
        // tuple index
        auto lexeme = _ast_ctx->current().lexeme;
        auto index = resolve_integer(lexeme);
        _ast_ctx->consume(); // index
        return std::make_unique<TupleIndexingExpression>(std::move(lft), index);
      }
      // method call (PathExprSegment) / field access (IDENTIFIER)
      // PathExprSegment can be a single identifier, so:
      auto segment = parsePathExprSegment();
      EXPECT_POINTER_NOT_EMPTY(segment);
      if(CHECK_TOKEN(kLParenthesis)) {
        // Indeed a method call
        _ast_ctx->consume();
        auto params_opt = parseCallParams();
        MATCH_TOKEN(kRParenthesis);
        return std::make_unique<MethodCallExpression>(std::move(lft), std::move(segment), std::move(params_opt));
      }
      // field access
      auto ident = segment->ident()->ident();
      if(ident == "super" || ident == "self" || ident == "Self" || ident == "crate" || ident == "$crate") {
        REPORT_FAILURE_AND_RETURN("FieldExpression: keyword as field");
      }
      return std::make_unique<FieldExpression>(std::move(lft), ident);
    }
    break;
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
      _ast_ctx->consume();
      auto rht = parseInfixExpression(new_pred);
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<ArithmeticOrLogicalExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kEqEq: case TokenType::kNe: case TokenType::kGt:
    case TokenType::kLt: case TokenType::kGe: case TokenType::kLe: {
      _ast_ctx->consume();
      auto rht = parseInfixExpression(new_pred);
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<ComparisonExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kAndAnd: case TokenType::kOrOr: {
      _ast_ctx->consume();
      auto rht = parseInfixExpression(new_pred);
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<LazyBooleanExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kEq: {
      _ast_ctx->consume();
      auto rht = parseInfixExpression(new_pred - 1); // right associative
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<AssignmentExpression>(std::move(lft), std::move(rht));
    } break;
    case TokenType::kPlusEq: case TokenType::kMinusEq: case TokenType::kStarEq:
    case TokenType::kSlashEq: case TokenType::kPercentEq: case TokenType::kCaretEq:
    case TokenType::kAndEq: case TokenType::kOrEq: case TokenType::kShlEq:
    case TokenType::kShrEq: {
      _ast_ctx->consume();
      auto rht = parseInfixExpression(new_pred - 1); // right associative
      EXPECT_POINTER_NOT_EMPTY(rht);
      lft = std::make_unique<CompoundAssignmentExpression>(token_to_operator(type), std::move(lft), std::move(rht));
    } break;
    case TokenType::kAs: {
      auto tnb = parseTypeNoBounds();
      EXPECT_POINTER_NOT_EMPTY(tnb);
      lft = std::make_unique<TypeCastExpression>(std::move(lft), std::move(tnb));
    } break;
    case TokenType::kDotDot: {
      _ast_ctx->consume();
      auto expr = parseExpression();
      if(!expr) {
        lft = std::make_unique<RangeFromExpr>(std::move(lft));
      } else {
        lft = std::make_unique<RangeExpr>(std::move(lft), std::move(expr));
      }
    } break;
    case TokenType::kDotDotEq: {
      _ast_ctx->consume();
      auto expr = parseExpression();
      EXPECT_POINTER_NOT_EMPTY(expr);;
      lft = std::make_unique<RangeInclusiveExpr>(std::move(lft), std::move(expr));
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
  auto ctt = _ast_ctx->current().type; // current token type
  auto ctc = get_token_category(ctt); // current token type category
  if(ctc == TokenTypeCat::kLiteral || ctt == TokenType::kTrue || ctt == TokenType::kFalse) {
    return parseLiteralExpression();
  }
  if(ctc == TokenTypeCat::kIdentifier) {
    return parsePathExpression();
  }
  switch(ctt) {
    // unary
  case TokenType::kMinus: case TokenType::kNot: {
    _ast_ctx->consume();
    auto expr = parseInfixExpression(static_cast<int>(Precedence::kPrefix));
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<NegationExpression>(token_to_operator(ctt), std::move(expr));
  }
  case TokenType::kStar: {
    _ast_ctx->consume();
    auto expr = parseInfixExpression(static_cast<int>(Precedence::kPrefix));
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
    auto expr = parseInfixExpression(static_cast<int>(Precedence::kPrefix));
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
    auto expr = parseInfixExpression(static_cast<int>(Precedence::kPrefix));
    EXPECT_POINTER_NOT_EMPTY(expr);
    // the first layer has no qualifier.
    return std::make_unique<BorrowExpression>(false, std::make_unique<BorrowExpression>(is_mut, std::move(expr)));
  }
  case TokenType::kDotDot: {
    // RangeFullExpr/RangeToExpr
    _ast_ctx->consume();
    auto expr = parseExpression();
    if(!expr) {
      return std::make_unique<RangeFullExpr>();
    } else {
      return std::make_unique<RangeToExpr>(std::move(expr));
    }
  }
  case TokenType::kDotDotEq: {
    // RangeToInclusiveExpr
    _ast_ctx->consume();
    auto expr = parseExpression();
    EXPECT_POINTER_NOT_EMPTY(expr);
    return std::make_unique<RangeToInclusiveExpr>(std::move(expr));
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
    // tuple or group expression.
    _ast_ctx->consume();
    if(CHECK_TOKEN(kRParenthesis)) {
      // empty tuple, or to say, a unit type
      _ast_ctx->consume();
      return std::make_unique<TupleExpression>(std::unique_ptr<TupleElements>());
    }
    auto expr1 = parseInfixExpression(static_cast<int>(Precedence::kLowest));
    EXPECT_POINTER_NOT_EMPTY(expr1);
    if(CHECK_TOKEN(kRParenthesis)) {
      // grouped expression
      _ast_ctx->consume();
      return std::make_unique<GroupedExpression>(std::move(expr1));
    }
    if(!CHECK_TOKEN(kComma)) {
      // failure.
      REPORT_FAILURE_AND_RETURN("Group/Tuple Expression: invalid syntax.");
    }
    // tuple expression
    // (Expr ',')+ Expr?
    std::vector<std::unique_ptr<Expression>> exprs;
    exprs.push_back(std::move(expr1));
    _ast_ctx->consume(); // ','
    while(!CHECK_TOKEN(kRParenthesis)) {
      auto expr = parseInfixExpression(static_cast<int>(Precedence::kLowest));
      EXPECT_POINTER_NOT_EMPTY(expr);
      exprs.push_back(std::move(expr));
      if(CHECK_TOKEN(kComma)) {
        _ast_ctx->consume();
      } else if(!CHECK_TOKEN(kRParenthesis)) {
        REPORT_FAILURE_AND_RETURN("Tuple Expression: Unexpected token");
      }
    }
    MATCH_TOKEN(kRParenthesis);
    return std::make_unique<TupleExpression>(std::make_unique<TupleElements>(std::move(exprs)));
  }
  case TokenType::kLSquareBracket: return parseArrayExpression();
  default: REPORT_FAILURE_AND_RETURN("Unexpected token in expression:" + token_type_to_string(ctt));
  }
  REPORT_FAILURE_AND_RETURN("Unexpected token in expression:" + token_type_to_string(ctt));
}

std::unique_ptr<Expression> Parser::parseExpression() {
  Backtracker tracker(*_ast_ctx);
  auto e = parseInfixExpression(static_cast<int>(Precedence::kLowest));
  EXPECT_POINTER_NOT_EMPTY(e);
  tracker.commit();
  return e;
}

std::unique_ptr<LiteralExpression> Parser::parseLiteralExpression() {
  using Prime = insomnia::rust_shard::type::TypePrime;
  Backtracker tracker(*_ast_ctx);
  auto current_token = _ast_ctx->current();
  _ast_ctx->consume();
  switch (current_token.type) {
  case TokenType::kIntegerLiteral: {
    auto lexeme = current_token.lexeme;
    std::size_t first_alpha = lexeme.find_first_of("iu");
    std::string value_str = remove_underscore(lexeme.substr(0, first_alpha));
    std::string_view suffix_str;
    if(first_alpha != std::string_view::npos)
      suffix_str = lexeme.substr(first_alpha);
    if(suffix_str.starts_with('i')) {
      std::int64_t value = std::stoll(value_str);
      Prime prime;
      if(suffix_str == "i8") prime = Prime::kI8;
      else if(suffix_str == "i16") prime = Prime::kI16;
      else if(suffix_str == "i32") prime = Prime::kI32;
      else if(suffix_str == "i64") prime = Prime::kI64;
      else if(suffix_str == "isize") prime = Prime::kISize;
      else REPORT_FAILURE_AND_RETURN("IntegerLiteral: unknown suffix " + std::string(suffix_str));
      tracker.commit();
      return std::make_unique<LiteralExpression>(prime, value);
    }
    if(suffix_str.starts_with('u')) {
      std::uint64_t value = std::stoull(value_str);
      Prime prime;
      if(suffix_str == "u8") prime = Prime::kU8;
      else if(suffix_str == "u16") prime = Prime::kU16;
      else if(suffix_str == "u32") prime = Prime::kU32;
      else if(suffix_str == "u64") prime = Prime::kU64;
      else if(suffix_str == "usize") prime = Prime::kUSize;
      else REPORT_FAILURE_AND_RETURN("IntegerLiteral: unknown suffix " + std::string(suffix_str));
      tracker.commit();
      return std::make_unique<LiteralExpression>(prime, value);
    }
    if(!suffix_str.empty()) {
      REPORT_FAILURE_AND_RETURN("IntegerLiteral: unknown suffix " + std::string(suffix_str));
    }
    // Treat as i32 as default
    std::int64_t value = std::stoll(value_str);
    tracker.commit();
    return std::make_unique<LiteralExpression>(Prime::kI32, value);
  }
  case TokenType::kFloatLiteral: {
    auto lexeme = current_token.lexeme;
    std::size_t first_alpha = lexeme.find_first_of("iu");
    std::string value_str = remove_underscore(lexeme.substr(0, first_alpha));
    std::string_view suffix_str;
    if(first_alpha != std::string_view::npos)
      suffix_str = lexeme.substr(first_alpha);
    if(suffix_str.starts_with('f')) {
      if(suffix_str == "i32") {
        tracker.commit();
        return std::make_unique<LiteralExpression>(Prime::kF32, std::stof(value_str));
      }
      if(suffix_str == "f64") {
        tracker.commit();
        return std::make_unique<LiteralExpression>(Prime::kF64, std::stod(value_str));
      }
      REPORT_FAILURE_AND_RETURN("FloatingPointLiteral: unknown suffix " + std::string(suffix_str));
    }
    if(!suffix_str.empty()) {
      REPORT_FAILURE_AND_RETURN("FloatingPointLiteral: unknown suffix " + std::string(suffix_str));
    }
    // Treat as f64 as default
    tracker.commit();
    return std::make_unique<LiteralExpression>(Prime::kF64, std::stod(value_str));
  }
  case TokenType::kStringLiteral: {
    tracker.commit();
    auto lexeme = current_token.lexeme;
    std::size_t l = 0, r = lexeme.length() - 1;
    while(lexeme[l] != '\"' && lexeme[l] != '\'') ++l;
    while(lexeme[r] != '\"' && lexeme[r] != '\'') --r;
    lexeme = lexeme.substr(l + 1, r - l - 1);
    return std::make_unique<LiteralExpression>(Prime::kString, std::string(lexeme));
  }
  case TokenType::kCharLiteral: {
    tracker.commit();
    return std::make_unique<LiteralExpression>(Prime::kChar, current_token.lexeme.front());
  }
  case TokenType::kTrue: {
    tracker.commit();
    return std::make_unique<LiteralExpression>(Prime::kBool, true);
  }
  case TokenType::kFalse: {
    tracker.commit();
    return std::make_unique<LiteralExpression>(Prime::kBool, false);
  }
  default:
    REPORT_FAILURE_AND_RETURN("parseLiteralExpression: Not a literal.");
  }
}

std::unique_ptr<PathExpression> Parser::parsePathExpression() {
  Backtracker tracker(*_ast_ctx);
  if(auto p = parsePathInExpression()) {
    tracker.commit();
    return p;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<PathInExpression> Parser::parsePathInExpression() {
  Backtracker tracker(*_ast_ctx);
  if(CHECK_TOKEN(kPathSep)) _ast_ctx->consume();
  std::vector<std::unique_ptr<PathExprSegment>> pess;
  auto pes1 = parsePathExprSegment();
  EXPECT_POINTER_NOT_EMPTY(pes1);
  pess.push_back(std::move(pes1));
  while(CHECK_TOKEN(kPathSep)) {
    _ast_ctx->consume();
    auto pes = parsePathExprSegment();
    EXPECT_POINTER_NOT_EMPTY(pes);
    pess.push_back(std::move(pes));
  }
  tracker.commit();
  return std::make_unique<PathInExpression>(std::move(pess));
}

std::unique_ptr<PathExprSegment> Parser::parsePathExprSegment() {
  Backtracker tracker(*_ast_ctx);
  auto pis = parsePathIdentSegment();
  EXPECT_POINTER_NOT_EMPTY(pis);
  tracker.commit();
  return std::make_unique<PathExprSegment>(std::move(pis));
}

std::unique_ptr<ArrayExpression> Parser::parseArrayExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLSquareBracket);
  auto ae = parseArrayElements();
  MATCH_TOKEN(kRSquareBracket);
  tracker.commit();
  return std::make_unique<ArrayExpression>(std::move(ae));
}

std::unique_ptr<ArrayElements> Parser::parseArrayElements() {
  Backtracker tracker(*_ast_ctx);
  if(auto e = parseExplicitArrayElements()) {
    tracker.commit();
    return e;
  }
  if(auto r = parseRepeatedArrayElements()) {
    tracker.commit();
    return r;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<ExplicitArrayElements> Parser::parseExplicitArrayElements() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Expression>> exprs;
  auto expr1 = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr1);
  exprs.push_back(std::move(expr1));
  while(!CHECK_TOKEN(kRSquareBracket)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRSquareBracket)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto expr = parseExpression();
    EXPECT_POINTER_NOT_EMPTY(expr);
    exprs.push_back(std::move(expr));
  }
  tracker.commit();
  return std::make_unique<ExplicitArrayElements>(std::move(exprs));
}

std::unique_ptr<RepeatedArrayElements> Parser::parseRepeatedArrayElements() {
  Backtracker tracker(*_ast_ctx);
  auto expr1 = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr1);
  MATCH_TOKEN(kSemi);
  auto expr2 = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr2);
  tracker.commit();
  return std::make_unique<RepeatedArrayElements>(std::move(expr1), std::move(expr2));
}

std::unique_ptr<IndexExpression> Parser::parseIndexExpression(std::unique_ptr<Expression> &&lft) {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLSquareBracket);
  auto rht = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(rht);
  MATCH_TOKEN(kRSquareBracket);
  tracker.commit();
  return std::make_unique<IndexExpression>(std::move(lft), std::move(rht));
}

std::unique_ptr<TupleIndexingExpression> Parser::parseTupleIndexingExpression(std::unique_ptr<Expression> &&lft) {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kDot);
  EXPECT_TOKEN(kIntegerLiteral);
  auto lexeme = _ast_ctx->current().lexeme;
  std::uint64_t index = resolve_integer(lexeme);
  _ast_ctx->consume(); // index
  tracker.commit();
  return std::make_unique<TupleIndexingExpression>(std::move(lft), index);
}

std::unique_ptr<StructExpression> Parser::parseStructExpression() {
  Backtracker tracker(*_ast_ctx);
  auto pie = parsePathInExpression();
  EXPECT_POINTER_NOT_EMPTY(pie);
  MATCH_TOKEN(kLCurlyBrace);
  auto sef = parseStructExprFields();
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<StructExpression>(std::move(pie), std::move(sef));
}

std::unique_ptr<StructExprFields> Parser::parseStructExprFields() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<StructExprField>> sefs;
  auto sef1 = parseStructExprField();
  EXPECT_POINTER_NOT_EMPTY(sef1);
  sefs.push_back(std::move(sef1));
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRCurlyBrace)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto sef = parseStructExprField();
    EXPECT_POINTER_NOT_EMPTY(sef);
    sefs.push_back(std::move(sef));
  }
  tracker.commit();
  return std::make_unique<StructExprFields>(std::move(sefs));
}

std::unique_ptr<StructExprField> Parser::parseStructExprField() {
  Backtracker tracker(*_ast_ctx);
  if(auto n = parseNamedStructExprField()) {
    tracker.commit();
    return n;
  }
  if(auto i = parseIndexStructExprField()) {
    tracker.commit();
    return i;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<NamedStructExprField> Parser::parseNamedStructExprField() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(kColon);
  auto expr = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr);
  tracker.commit();
  return std::make_unique<NamedStructExprField>(ident, std::move(expr));
}

std::unique_ptr<IndexStructExprField> Parser::parseIndexStructExprField() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(kIntegerLiteral);
  auto lexeme = _ast_ctx->current().lexeme;
  std::uint64_t index = resolve_integer(lexeme);
  _ast_ctx->consume(); // index
  MATCH_TOKEN(kColon);
  auto expr = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr);
  tracker.commit();
  return std::make_unique<IndexStructExprField>(index, std::move(expr));
}

std::unique_ptr<CallExpression> Parser::parseCallExpression(std::unique_ptr<Expression> &&lft) {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLParenthesis);
  auto cp_opt = parseCallParams();
  MATCH_TOKEN(kRParenthesis);
  tracker.commit();
  return std::make_unique<CallExpression>(std::move(lft), std::move(cp_opt));
}

std::unique_ptr<CallParams> Parser::parseCallParams() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Expression>> exprs;
  auto expr1 = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr1);
  exprs.push_back(std::move(expr1));
  while(!CHECK_TOKEN(kRParenthesis)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRParenthesis)) break;;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto expr = parseExpression();
    EXPECT_POINTER_NOT_EMPTY(expr);
    exprs.push_back(std::move(expr));
  }
  tracker.commit();
  return std::make_unique<CallParams>(std::move(exprs));
}

std::unique_ptr<ContinueExpression> Parser::parseContinueExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kContinue);
  tracker.commit();
  return std::make_unique<ContinueExpression>();
}

std::unique_ptr<BreakExpression> Parser::parseBreakExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kBreak);
  auto expr_opt = parseExpression();
  tracker.commit();
  return std::make_unique<BreakExpression>(std::move(expr_opt));
}

std::unique_ptr<ReturnExpression> Parser::parseReturnExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kReturn);
  auto expr_opt = parseExpression();
  tracker.commit();
  return std::make_unique<ReturnExpression>(std::move(expr_opt));
}

std::unique_ptr<UnderscoreExpression> Parser::parseUnderscoreExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kUnderscore);
  tracker.commit();
  return std::make_unique<UnderscoreExpression>();
}

std::unique_ptr<BlockExpression> Parser::parseBlockExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLCurlyBrace);
  auto ss_opt = parseStatements();
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<BlockExpression>(std::move(ss_opt));
}

std::unique_ptr<Statements> Parser::parseStatements() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Statement>> ss;
  while(!CHECK_TOKEN(kRCurlyBrace)) {
    auto s = parseStatement();
    if(!s) break;
    ss.push_back(std::move(s));
  }
  std::unique_ptr<ExpressionWithoutBlock> ewb;
  if(!CHECK_TOKEN(kRCurlyBrace)) {
    ewb = std::unique_ptr<ExpressionWithoutBlock>(dynamic_cast<ExpressionWithoutBlock*>(parseExpression().release()));
    EXPECT_POINTER_NOT_EMPTY(ewb);
  }
  EXPECT_TOKEN(kRCurlyBrace);
  if(ss.empty() && !ewb) {
    REPORT_FAILURE_AND_RETURN("An empty block");
  }
  tracker.commit();
  return std::make_unique<Statements>(std::move(ss), std::move(ewb));
}

std::unique_ptr<Statement> Parser::parseStatement() {
  Backtracker tracker(*_ast_ctx);
  if(auto e = parseEmptyStatement()) {
    tracker.commit();
    return e;
  }
  if(auto i = parseItemStatement()) {
    tracker.commit();
    return i;
  }
  if(auto l = parseLetStatement()) {
    tracker.commit();
    return l;
  }
  if(auto e = parseExpressionStatement()) {
    tracker.commit();
    return e;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<EmptyStatement> Parser::parseEmptyStatement() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kSemi);
  tracker.commit();
  return std::make_unique<EmptyStatement>();
}

std::unique_ptr<ItemStatement> Parser::parseItemStatement() {
  Backtracker tracker(*_ast_ctx);
  auto i = parseItem();
  EXPECT_POINTER_NOT_EMPTY(i);
  tracker.commit();
  return std::make_unique<ItemStatement>(std::move(i));
}

std::unique_ptr<LetStatement> Parser::parseLetStatement() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLet);
  auto pnta = parsePatternNoTopAlt();
  EXPECT_POINTER_NOT_EMPTY(pnta);
  std::unique_ptr<Type> t;
  if(CHECK_TOKEN(kColon)) {
    _ast_ctx->consume();
    t = parseType();
    EXPECT_POINTER_NOT_EMPTY(t);
  }
  std::unique_ptr<Expression> e;
  if(CHECK_TOKEN(kEq)) {
    _ast_ctx->consume();
    e = parseExpression();
    EXPECT_POINTER_NOT_EMPTY(e);
  }
  tracker.commit();
  return std::make_unique<LetStatement>(std::move(pnta), std::move(t), std::move(e));
}

std::unique_ptr<ExpressionStatement> Parser::parseExpressionStatement() {
  Backtracker tracker(*_ast_ctx);
  auto expr = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr);
  if(expr->has_block()) {
    if(CHECK_TOKEN(kSemi)) _ast_ctx->consume();
  } else {
    MATCH_TOKEN(kSemi);
  }
  tracker.commit();
  return std::make_unique<ExpressionStatement>(std::move(expr));
}

std::unique_ptr<LoopExpression> Parser::parseLoopExpression() {
  Backtracker tracker(*_ast_ctx);
  if(auto i = parseInfiniteLoopExpression()) {
    tracker.commit();
    return i;
  }
  if(auto p = parsePredicateLoopExpression()) {
    tracker.commit();
    return p;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<InfiniteLoopExpression> Parser::parseInfiniteLoopExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLoop);
  auto be = parseBlockExpression();
  EXPECT_POINTER_NOT_EMPTY(be);
  tracker.commit();
  return std::make_unique<InfiniteLoopExpression>(std::move(be));
}

std::unique_ptr<PredicateLoopExpression> Parser::parsePredicateLoopExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kWhile);
  auto c = parseConditions();
  EXPECT_POINTER_NOT_EMPTY(c);
  auto be = parseBlockExpression();
  EXPECT_POINTER_NOT_EMPTY(be);
  tracker.commit();
  return std::make_unique<PredicateLoopExpression>(std::move(c), std::move(be));
}

std::unique_ptr<IfExpression> Parser::parseIfExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kIf);
  auto c = parseConditions();
  EXPECT_POINTER_NOT_EMPTY(c);
  auto be = parseBlockExpression();
  EXPECT_POINTER_NOT_EMPTY(be);
  if(!CHECK_TOKEN(kElse)) {
    tracker.commit();
    return std::make_unique<IfExpression>(
      std::move(c), std::move(be)
    );
  }
  _ast_ctx->consume(); // "else"
  if(auto b = parseBlockExpression()) {
    tracker.commit();
    return std::make_unique<IfExpression>(
      std::move(c), std::move(be), std::move(b)
    );
  }
  if(auto i = parseIfExpression()) {
    tracker.commit();
    return std::make_unique<IfExpression>(
      std::move(c), std::move(be), std::move(i)
    );
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<Conditions> Parser::parseConditions() {
  Backtracker tracker(*_ast_ctx);
  auto expr = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr);
  tracker.commit();
  return std::make_unique<Conditions>(std::move(expr));
}

std::unique_ptr<MatchExpression> Parser::parseMatchExpression() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kMatch);
  auto expr = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr);
  MATCH_TOKEN(kLCurlyBrace);
  auto ma = parseMatchArms();
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<MatchExpression>(std::move(expr), std::move(ma));
}

std::unique_ptr<MatchArms> Parser::parseMatchArms() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::pair<std::unique_ptr<MatchArm>, std::unique_ptr<Expression>>> arms;
  while(CHECK_TOKEN(kRCurlyBrace)) {
    auto arm = parseMatchArm();
    EXPECT_POINTER_NOT_EMPTY(arm);
    MATCH_TOKEN(kFatArrow);
    auto expr = parseExpression();
    arms.emplace_back(std::move(arm), std::move(expr));
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRCurlyBrace)) break;
    if(!expr->has_block()) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
  }
  tracker.commit();
  return std::make_unique<MatchArms>(std::move(arms));
}

std::unique_ptr<MatchArm> Parser::parseMatchArm() {
  Backtracker tracker(*_ast_ctx);
  auto p = parsePattern();
  EXPECT_POINTER_NOT_EMPTY(p);
  auto g = parseMatchArmGuard();
  tracker.commit();
  return std::make_unique<MatchArm>(std::move(p), std::move(g));
}

std::unique_ptr<MatchArmGuard> Parser::parseMatchArmGuard() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kIf);
  auto expr = parseExpression();
  EXPECT_POINTER_NOT_EMPTY(expr);
  tracker.commit();
  return std::make_unique<MatchArmGuard>(std::move(expr));
}

std::unique_ptr<Pattern> Parser::parsePattern() {
  Backtracker tracker(*_ast_ctx);
  if(CHECK_TOKEN(kOr)) _ast_ctx->consume();
  std::vector<std::unique_ptr<PatternNoTopAlt>> ps;
  auto p1 = parsePatternNoTopAlt();
  EXPECT_POINTER_NOT_EMPTY(p1);
  ps.push_back(std::move(p1));
  while(CHECK_TOKEN(kOr)) {
    _ast_ctx->consume();
    auto p = parsePatternNoTopAlt();
    ps.push_back(std::move(p));
  }
  tracker.commit();
  return std::make_unique<Pattern>(std::move(ps));
}

std::unique_ptr<PatternNoTopAlt> Parser::parsePatternNoTopAlt() {
  Backtracker tracker(*_ast_ctx);
  if(auto p = parsePatternWithoutRange()) {
    tracker.commit();
    return p;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<PatternWithoutRange> Parser::parsePatternWithoutRange() {
  Backtracker tracker(*_ast_ctx);
  if(auto l = parseLiteralPattern()) {
    tracker.commit();
    return l;
  }
  if(auto i = parseIdentifierPattern()) {
    tracker.commit();
    return i;
  }
  if(auto w = parseWildcardPattern()) {
    tracker.commit();
    return w;
  }
  if(auto r = parseReferencePattern()) {
    tracker.commit();
    return r;
  }
  if(auto s = parseStructPattern()) {
    tracker.commit();
    return s;
  }
  if(auto t = parseTuplePattern()) {
    tracker.commit();
    return t;
  }
  if(auto g = parseGroupedPattern()) {
    tracker.commit();
    return g;
  }
  if(auto s = parseSlicePattern()) {
    tracker.commit();
    return s;
  }
  if(auto p = parsePathPattern()) {
    tracker.commit();
    return p;
  }
  REPORT_PARSE_FAILURE_AND_RETURN();
}

std::unique_ptr<LiteralPattern> Parser::parseLiteralPattern() {
  Backtracker tracker(*_ast_ctx);
  bool is_neg = false;
  if(CHECK_TOKEN(kMinus)) {
    _ast_ctx->consume();
    is_neg = true;
  }
  auto l = parseLiteralExpression();
  EXPECT_POINTER_NOT_EMPTY(l);
  tracker.commit();
  return std::make_unique<LiteralPattern>(is_neg, std::move(l));
}

std::unique_ptr<IdentifierPattern> Parser::parseIdentifierPattern() {
  Backtracker tracker(*_ast_ctx);
  bool is_ref = false;
  if(CHECK_TOKEN(kRef)) {
    _ast_ctx->consume();
    is_ref = true;
  }
  bool is_mut = false;
  if(CHECK_TOKEN(kMut)) {
    _ast_ctx->consume();
    is_mut = true;
  }
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  std::unique_ptr<PatternNoTopAlt> p_opt;
  if(CHECK_TOKEN(kAt)) {
    _ast_ctx->consume();
    p_opt = parsePatternNoTopAlt();
    EXPECT_POINTER_NOT_EMPTY(p_opt);
  }
  tracker.commit();
  return std::make_unique<IdentifierPattern>(is_ref, is_mut, ident, std::move(p_opt));
}

std::unique_ptr<WildcardPattern> Parser::parseWildcardPattern() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kUnderscore);
  tracker.commit();
  return std::make_unique<WildcardPattern>();
}

std::unique_ptr<ReferencePattern> Parser::parseReferencePattern() {
  Backtracker tracker(*_ast_ctx);
  if(!CHECK_TOKEN(kAnd) && !CHECK_TOKEN(kAndAnd)) {
    REPORT_FAILURE_AND_RETURN("Not a reference");
  }
  bool is_one = CHECK_TOKEN(kAnd);
  _ast_ctx->consume();
  bool is_mut = false;
  if(CHECK_TOKEN(kMut)) {
    _ast_ctx->consume();
    is_mut = true;
  }
  auto p = parsePatternWithoutRange();
  EXPECT_POINTER_NOT_EMPTY(p);
  tracker.commit();
  if(is_one) {
    return std::make_unique<ReferencePattern>(is_mut, std::move(p));
  } else {
    return std::make_unique<ReferencePattern>(false, std::make_unique<ReferencePattern>(is_mut, std::move(p)));
  }
}

std::unique_ptr<StructPattern> Parser::parseStructPattern() {
  Backtracker tracker(*_ast_ctx);
  auto p = parsePathInExpression();
  EXPECT_POINTER_NOT_EMPTY(p);
  MATCH_TOKEN(kLCurlyBrace);
  auto s_opt = parseStructPatternElements();
  MATCH_TOKEN(kRCurlyBrace);
  tracker.commit();
  return std::make_unique<StructPattern>(std::move(p), std::move(s_opt));
}

std::unique_ptr<StructPatternElements> Parser::parseStructPatternElements() {
  Backtracker tracker(*_ast_ctx);
  auto ss = parseStructPatternFields();
  EXPECT_POINTER_NOT_EMPTY(ss);
  if(CHECK_TOKEN(kComma)) _ast_ctx->consume();
  tracker.commit();
  return std::make_unique<StructPatternElements>(std::move(ss));
}

std::unique_ptr<StructPatternFields> Parser::parseStructPatternFields() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<StructPatternField>> ss;
  auto s1 = parseStructPatternField();
  EXPECT_POINTER_NOT_EMPTY(s1);
  ss.push_back(std::move(s1));
  while(CHECK_TOKEN(kComma)) {
    _ast_ctx->consume();
    auto s = parseStructPatternField();
    EXPECT_POINTER_NOT_EMPTY(s);
    ss.push_back(std::move(s));
  }
  tracker.commit();
  return std::make_unique<StructPatternFields>(std::move(ss));
}

std::unique_ptr<StructPatternField> Parser::parseStructPatternField() {
  Backtracker tracker(*_ast_ctx);
  EXPECT_TOKEN(kIdentifier);
  auto ident = _ast_ctx->current().lexeme;
  _ast_ctx->consume();
  MATCH_TOKEN(kColon);
  auto p = parsePattern();
  EXPECT_POINTER_NOT_EMPTY(p);
  tracker.commit();
  return std::make_unique<StructPatternField>(ident, std::move(p));
}

std::unique_ptr<TuplePattern> Parser::parseTuplePattern() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLParenthesis);
  auto t = parseTuplePatternItems();
  MATCH_TOKEN(kRParenthesis);
  tracker.commit();
  return std::make_unique<TuplePattern>(std::move(t));
}

std::unique_ptr<TuplePatternItems> Parser::parseTuplePatternItems() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Pattern>> ps;
  auto p1 = parsePattern();
  EXPECT_POINTER_NOT_EMPTY(p1);
  ps.push_back(std::move(p1));
  MATCH_TOKEN(kComma);
  if(CHECK_TOKEN(kRParenthesis)) {
    tracker.commit();
    return std::make_unique<TuplePatternItems>(std::move(ps));
  }
  p1 = parsePattern();
  EXPECT_POINTER_NOT_EMPTY(p1);
  ps.push_back(std::move(p1));
  while(!CHECK_TOKEN(kRParenthesis)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRParenthesis)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto p = parsePattern();
    EXPECT_POINTER_NOT_EMPTY(p);
    ps.push_back(std::move(p));
  }
  tracker.commit();
  return std::make_unique<TuplePatternItems>(std::move(ps));
}

std::unique_ptr<GroupedPattern> Parser::parseGroupedPattern() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLParenthesis);
  auto p = parsePattern();
  EXPECT_POINTER_NOT_EMPTY(p);
  MATCH_TOKEN(kRParenthesis);
  tracker.commit();
  return std::make_unique<GroupedPattern>(std::move(p));
}

std::unique_ptr<SlicePattern> Parser::parseSlicePattern() {
  Backtracker tracker(*_ast_ctx);
  MATCH_TOKEN(kLSquareBracket);
  auto s_opt = parseSlicePatternItems();
  MATCH_TOKEN(kRSquareBracket);
  tracker.commit();
  return std::make_unique<SlicePattern>(std::move(s_opt));
}

std::unique_ptr<SlicePatternItems> Parser::parseSlicePatternItems() {
  Backtracker tracker(*_ast_ctx);
  std::vector<std::unique_ptr<Pattern>> ps;
  auto p1 = parsePattern();
  EXPECT_POINTER_NOT_EMPTY(p1);
  ps.push_back(std::move(p1));
  while(!CHECK_TOKEN(kRSquareBracket)) {
    bool has_comma = CHECK_TOKEN(kComma);
    if(has_comma) _ast_ctx->consume();
    if(CHECK_TOKEN(kRSquareBracket)) break;
    if(!has_comma) REPORT_MISSING_TOKEN_AND_RETURN(kComma);
    auto p = parsePattern();
    EXPECT_POINTER_NOT_EMPTY(p);
    ps.push_back(std::move(p));
  }
  tracker.commit();
  return std::make_unique<SlicePatternItems>(std::move(ps));
}

std::unique_ptr<PathPattern> Parser::parsePathPattern() {
  Backtracker tracker(*_ast_ctx);
  auto p = parsePathExpression();
  EXPECT_POINTER_NOT_EMPTY(p);
  tracker.commit();
  return std::make_unique<PathPattern>(std::move(p));
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