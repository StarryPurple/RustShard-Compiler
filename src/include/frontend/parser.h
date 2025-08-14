#ifndef INSOMNIA_PARSER_H
#define INSOMNIA_PARSER_H
#include "ast.h"
#include "lexer.h"

namespace insomnia::ast {


class Parser {
  class Context; // Token flow
  class Backtracker;
  std::unique_ptr<Context> _ast_ctx;
  bool _is_good;

  std::unique_ptr<Crate> _crate;

  // returns an empty pointer as an error signal.

  std::unique_ptr<Crate> parse_crate();
  std::unique_ptr<Item> parse_item();
  std::unique_ptr<VisItem> parse_vis_item();
  std::unique_ptr<Function> parse_function();
  std::unique_ptr<Struct> parse_struct();
  std::unique_ptr<Enumeration> parse_enumeration();
  std::unique_ptr<ConstantItem> parse_constant_item();
  std::unique_ptr<Trait> parse_trait();
  std::unique_ptr<Implementation> parse_implementation();
  std::unique_ptr<FunctionParameters> parse_function_parameters();
  std::unique_ptr<FunctionParam> parse_function_param();
  std::unique_ptr<FunctionParamPattern> parse_function_param_pattern();
  std::unique_ptr<SelfParam> parse_self_param();
  std::unique_ptr<Type> parse_type();
  std::unique_ptr<TypeNoBounds> parse_type_no_bounds();
  std::unique_ptr<ParenthesizedType> parse_parenthesized_type();
  std::unique_ptr<TupleType> parse_tuple_type();
  std::unique_ptr<ReferenceType> parse_reference_type();
  std::unique_ptr<ArrayType> parse_array_type();
  std::unique_ptr<SliceType> parse_slice_type();
  std::unique_ptr<StructStruct> parse_struct_struct();
  std::unique_ptr<StructFields> parse_struct_fields();
  std::unique_ptr<StructField> parse_struct_field();
  std::unique_ptr<EnumItems> parse_enum_items();
  std::unique_ptr<EnumItem> parse_enum_item();
  std::unique_ptr<EnumItemDiscriminant> parse_enum_item_discriminant();
  std::unique_ptr<AssociatedItem> parse_associated_item();
  std::unique_ptr<InherentImpl> parse_inherent_impl();
  std::unique_ptr<TraitImpl> parse_trait_impl();
  std::unique_ptr<TypePath> parse_type_path();
  std::unique_ptr<TypePathSegment> parse_type_path_segment();
  std::unique_ptr<PathIdentSegment> parse_path_ident_segment();
  std::unique_ptr<Expression> parse_expression();
  std::unique_ptr<ExpressionWithoutBlock> parse_expression_without_block();
  std::unique_ptr<LiteralExpression> parse_literal_expression();
  std::unique_ptr<PathExpression> parse_path_expression();
  std::unique_ptr<PathInExpression> parse_path_in_expression();
  std::unique_ptr<PathExprSegment> parse_path_expr_segment();
  std::unique_ptr<OperatorExpression> parse_operator_expression();
  std::unique_ptr<BorrowExpression> parse_borrow_expression();
  std::unique_ptr<DereferenceExpression> parse_dereference_expression();
  std::unique_ptr<NegationExpression> parse_negation_expression();
  std::unique_ptr<ArithmeticOrLogicalExpression> parse_arithmetic_or_logical_expression();
  std::unique_ptr<ComparisonExpression> parse_comparison_expression();
  std::unique_ptr<LazyBooleanExpression> parse_lazy_boolean_expression();
  std::unique_ptr<TypeCastExpression> parse_type_cast_expression();
  std::unique_ptr<AssignmentExpression> parse_assignment_expression();
  std::unique_ptr<CompoundAssignmentExpression> parse_compound_assignment_expression();
  std::unique_ptr<GroupedExpression> parse_grouped_expression();
  std::unique_ptr<ArrayExpression> parse_array_expression();
  std::unique_ptr<ArrayElements> parse_array_elements();
  std::unique_ptr<IndexExpression> parse_index_expression();
  std::unique_ptr<TupleExpression> parse_tuple_expression();
  std::unique_ptr<TupleElements> parse_tuple_elements();
  std::unique_ptr<TupleIndexingExpression> parse_tuple_indexing_expression();
  std::unique_ptr<StructExpression> parse_struct_expression();
  std::unique_ptr<StructExprFields> parse_struct_expr_fields();
  std::unique_ptr<StructExprField> parse_struct_expr_field();
  std::unique_ptr<StructBase> parse_struct_base();
  std::unique_ptr<CallExpression> parse_call_expression();
  std::unique_ptr<CallParams> parse_call_params();
  std::unique_ptr<MethodCallExpression> parse_method_call_expression();
  std::unique_ptr<FieldExpression> parse_field_expression();
  std::unique_ptr<ContinueExpression> parse_continue_expression();
  std::unique_ptr<BreakExpression> parse_break_expression();
  std::unique_ptr<RangeExpression> parse_range_expression();
  std::unique_ptr<RangeExpr> parse_range_expr();
  std::unique_ptr<RangFromExpr> parse_rang_from_expr();
  std::unique_ptr<RangeToExpr> parse_range_to_expr();
  std::unique_ptr<RangeFullExpr> parse_range_full_expr();
  std::unique_ptr<RangeInclusiveExpr> parse_range_inclusive_expr();
  std::unique_ptr<RangeToInclusiveExpr> parse_range_to_inclusive_expr();
  std::unique_ptr<ReturnExpression> parse_return_expression();
  std::unique_ptr<UnderscoreExpression> parse_underscore_expression();
  std::unique_ptr<ExpressionWithBlock> parse_expression_with_block();
  std::unique_ptr<BlockExpression> parse_block_expression();
  std::unique_ptr<Statements> parse_statements();
  std::unique_ptr<Statement> parse_statement();
  std::unique_ptr<LetStatement> parse_let_statement();
  std::unique_ptr<ExpressionStatement> parse_expression_statement();
  std::unique_ptr<LoopExpression> parse_loop_expression();
  std::unique_ptr<InfiniteLoopExpression> parse_infinite_loop_expression();
  std::unique_ptr<PredicateLoopExpression> parse_predicate_loop_expression();
  std::unique_ptr<IfExpression> parse_if_expression();
  std::unique_ptr<Conditions> parse_conditions();
  std::unique_ptr<MatchExpression> parse_match_expression();
  std::unique_ptr<MatchArms> parse_match_arms();
  std::unique_ptr<MatchArm> parse_match_arm();
  std::unique_ptr<MatchArmGuard> parse_match_arm_guard();
  std::unique_ptr<Pattern> parse_pattern();
  std::unique_ptr<PatternNoTopAlt> parse_pattern_no_top_alt();
  std::unique_ptr<PatternWithoutRange> parse_pattern_without_range();
  std::unique_ptr<LiteralPattern> parse_literal_pattern();
  std::unique_ptr<IdentifierPattern> parse_identifier_pattern();
  std::unique_ptr<WildcardPattern> parse_wildcard_pattern();
  std::unique_ptr<RestPattern> parse_rest_pattern();
  std::unique_ptr<ReferencePattern> parse_reference_patter();
  std::unique_ptr<StructPattern> parse_struct_pattern();
  std::unique_ptr<StructPatternElements> parse_struct_pattern_elements();
  std::unique_ptr<StructPatternEtCetera> parse_struct_pattern_et_cetera();
  std::unique_ptr<StructPatternFields> parse_struct_pattern_fields();
  std::unique_ptr<StructPatternField> parse_struct_pattern_field();
  std::unique_ptr<TuplePattern> parse_tuple_pattern();
  std::unique_ptr<TuplePatternItems> parse_tuple_pattern_items();
  std::unique_ptr<GroupedPattern> parse_grouped_pattern();
  std::unique_ptr<SlicePattern> parse_slice_pattern();
  std::unique_ptr<SlicePatternItems> parse_slice_pattern_items();
  std::unique_ptr<PathPattern> parse_path_pattern();

public:
  Parser() = default;
  explicit Parser(Lexer &lexer) { parse(lexer); }

  void parse(Lexer &lexer);

  [[nodiscard]] explicit operator bool() const { return _is_good; }
  [[nodiscard]] bool is_good() const { return _is_good; }
};

}

#endif // INSOMNIA_PARSER_H