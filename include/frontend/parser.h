#ifndef RUST_SHARD_FRONTEND_PARSER_H
#define RUST_SHARD_FRONTEND_PARSER_H

#include "ast.h"
#include "lexer.h"

#define PARSE_FUNCTION_GEN_METHOD(Node) \
  std::unique_ptr<Node> parse##Node();

namespace insomnia::rust_shard::ast {

class Parser {
public:
  Parser();
  ~Parser();
  explicit Parser(const Lexer &lexer);
  void parseAll(const Lexer &lexer);
  explicit operator bool() const { return _is_good; }
  bool is_good() const { return _is_good; }
  std::string error_msg() const;
  ASTTree release_tree();

private:
  class Backtracker;
  class Context;
  bool _is_good = false;
  std::unique_ptr<Context> _ast_ctx;
  std::unique_ptr<Crate> _crate;

  // returns an empty pointer as failure signal.

  // INSOMNIA_RUST_SHARD_AST_NODES_LIST(PARSE_FUNCTION_GEN_METHOD)

  // Pratt Expression Parsing helper functions.

  std::unique_ptr<Expression> parseInfixExpression(int precedence, TokenType delim);
  std::unique_ptr<Expression> parsePrefixExpression(TokenType delim);

  std::unique_ptr<Crate> parseCrate();
  std::unique_ptr<Item> parseItem();
  std::unique_ptr<VisItem> parseVisItem();
  std::unique_ptr<Function> parseFunction();
  std::unique_ptr<FunctionBodyExpr> parseFunctionBodyExpr();
  std::unique_ptr<FunctionParameters> parseFunctionParameters();
  std::unique_ptr<FunctionParam> parseFunctionParam();
  std::unique_ptr<FunctionParamPattern> parseFunctionParamPattern();
  std::unique_ptr<FunctionParamType> parseFunctionParamType();
  std::unique_ptr<SelfParam> parseSelfParam();
  std::unique_ptr<Type> parseType();
  std::unique_ptr<TypeNoBounds> parseTypeNoBounds();
  std::unique_ptr<ParenthesizedType> parseParenthesizedType();
  std::unique_ptr<TupleType> parseTupleType();
  std::unique_ptr<ReferenceType> parseReferenceType();
  std::unique_ptr<ArrayType> parseArrayType();
  std::unique_ptr<SliceType> parseSliceType();
  std::unique_ptr<Struct> parseStruct();
  std::unique_ptr<StructStruct> parseStructStruct();
  std::unique_ptr<StructFields> parseStructFields();
  std::unique_ptr<StructField> parseStructField();
  std::unique_ptr<Enumeration> parseEnumeration();
  std::unique_ptr<EnumItems> parseEnumItems();
  std::unique_ptr<EnumItem> parseEnumItem();
  std::unique_ptr<EnumItemDiscriminant> parseEnumItemDiscriminant();
  std::unique_ptr<ConstantItem> parseConstantItem();
  std::unique_ptr<Trait> parseTrait();
  std::unique_ptr<AssociatedItem> parseAssociatedItem();
  std::unique_ptr<AssociatedTypeAlias> parseAssociatedTypeAlias();
  std::unique_ptr<AssociatedConstantItem> parseAssociatedConstantItem();
  std::unique_ptr<AssociatedFunction> parseAssociatedFunction();
  std::unique_ptr<TypeAlias> parseTypeAlias();
  std::unique_ptr<Implementation> parseImplementation();
  std::unique_ptr<InherentImpl> parseInherentImpl();
  std::unique_ptr<TraitImpl> parseTraitImpl();
  std::unique_ptr<TypePath> parseTypePath();
  std::unique_ptr<TypePathSegment> parseTypePathSegment();
  std::unique_ptr<PathIdentSegment> parsePathIdentSegment();
  std::unique_ptr<Expression> parseExpression(TokenType delim = TokenType::kInvalid);
  std::unique_ptr<LiteralExpression> parseLiteralExpression();
  std::unique_ptr<PathExpression> parsePathExpression();
  std::unique_ptr<PathInExpression> parsePathInExpression();
  std::unique_ptr<PathExprSegment> parsePathExprSegment();
  std::unique_ptr<ArrayExpression> parseArrayExpression();
  std::unique_ptr<ArrayElements> parseArrayElements();
  std::unique_ptr<ExplicitArrayElements> parseExplicitArrayElements();
  std::unique_ptr<RepeatedArrayElements> parseRepeatedArrayElements();
  std::unique_ptr<IndexExpression> parseIndexExpression(std::unique_ptr<Expression> &&lft);
  std::unique_ptr<TupleIndexingExpression> parseTupleIndexingExpression(std::unique_ptr<Expression> &&lft);
  std::unique_ptr<StructExpression> parseStructExpression();
  std::unique_ptr<StructExprFields> parseStructExprFields();
  std::unique_ptr<StructExprField> parseStructExprField();
  std::unique_ptr<NamedStructExprField> parseNamedStructExprField();
  std::unique_ptr<IndexStructExprField> parseIndexStructExprField();
  std::unique_ptr<CallExpression> parseCallExpression(std::unique_ptr<Expression> &&lft);
  std::unique_ptr<CallParams> parseCallParams();
  std::unique_ptr<ContinueExpression> parseContinueExpression();
  std::unique_ptr<BreakExpression> parseBreakExpression();
  std::unique_ptr<ReturnExpression> parseReturnExpression();
  std::unique_ptr<UnderscoreExpression> parseUnderscoreExpression();
  std::unique_ptr<BlockExpression> parseBlockExpression();
  std::unique_ptr<Statements> parseStatements();
  std::unique_ptr<Statement> parseStatement();
  std::unique_ptr<EmptyStatement> parseEmptyStatement();
  std::unique_ptr<ItemStatement> parseItemStatement();
  std::unique_ptr<LetStatement> parseLetStatement();
  std::unique_ptr<ExpressionStatement> parseExpressionStatement();
  std::unique_ptr<LoopExpression> parseLoopExpression();
  std::unique_ptr<InfiniteLoopExpression> parseInfiniteLoopExpression();
  std::unique_ptr<PredicateLoopExpression> parsePredicateLoopExpression();
  std::unique_ptr<IfExpression> parseIfExpression();
  std::unique_ptr<Conditions> parseConditions();
  std::unique_ptr<MatchExpression> parseMatchExpression();
  std::unique_ptr<MatchArms> parseMatchArms();
  std::unique_ptr<MatchArm> parseMatchArm();
  std::unique_ptr<MatchArmGuard> parseMatchArmGuard();
  std::unique_ptr<Pattern> parsePattern();
  std::unique_ptr<PatternNoTopAlt> parsePatternNoTopAlt();
  std::unique_ptr<PatternWithoutRange> parsePatternWithoutRange();
  std::unique_ptr<LiteralPattern> parseLiteralPattern();
  std::unique_ptr<IdentifierPattern> parseIdentifierPattern();
  std::unique_ptr<WildcardPattern> parseWildcardPattern();
  std::unique_ptr<ReferencePattern> parseReferencePattern();
  std::unique_ptr<StructPattern> parseStructPattern();
  std::unique_ptr<StructPatternElements> parseStructPatternElements();
  std::unique_ptr<StructPatternFields> parseStructPatternFields();
  std::unique_ptr<StructPatternField> parseStructPatternField();
  std::unique_ptr<TuplePattern> parseTuplePattern();
  std::unique_ptr<TuplePatternItems> parseTuplePatternItems();
  std::unique_ptr<GroupedPattern> parseGroupedPattern();
  std::unique_ptr<SlicePattern> parseSlicePattern();
  std::unique_ptr<SlicePatternItems> parseSlicePatternItems();
  std::unique_ptr<PathPattern> parsePathPattern();
};

}

#undef PARSE_FUNCTION_GEN_METHOD
#endif // RUST_SHARD_FRONTEND_PARSER_H