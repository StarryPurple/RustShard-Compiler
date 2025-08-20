Single file, no modules, no crates.

No generics/templates/where clauses, no lifetimes, no labels, no closures.

Usage of '#': The rest of the line is taken as comments, unless another '#' emerges to stop it.

Feels like a "/*" with both "*/" and '\n' as escapes.

The super grammar module is `Crate`.

```

Crate -> 
    Item*

Item -> 
    VisItem

VisItem ->
      Function
    | TypeAlias
    | Struct
    | Enumeration
    | ConstantItem
    | Trait
    | Implementation

Function ->
    "const"? "fn" IDENTIFIER #GenericParams?#
    '(' FunctionParameters? ')'
    ("->" Type)? #WhereClause?# (BlockExpression | ';')
    
# GenericParams -> ...

# Followed by a ')'
FunctionParameters ->
      SelfParam ','? 
    | (SelfParam ',')? FunctionParam (',' FunctionParam)* ','?

# Actually you can use this:
# SelfParam? (',' FunctionParam)* ','?
# And there must be something
    
# No unsafe "..."
FunctionParam ->
      FunctionParamPattern 
    | FunctionParamType

# No unsafe "..."
FunctionParamPattern ->
    PatternNoTopAlt ':' Type

FunctionParamType ->
    Type
    
SelfParam ->
    '&'? "mut"? "self" (':' Type)?

Type -> 
      TypeNoBounds
    # | ImplTraitType
    # | TraitObjectType
    
    
TypeNoBounds ->
      ParenthesizedType
    | TupleType
    | ReferenceType
    | ArrayType
    | SliceType
    
ParenthesizedType ->
    '(' Type ')'

TupleType ->
      '(' ')'
    | '(' (Type ',')+ Type? ')'
    
ReferenceType ->
    '&' "mut"? TypeNoBounds

# Expression here shall be constant
ArrayType ->
    '[' Type ';' Expression ']'
    
SliceType ->
    '[' Type ']'
    
# WhereClause ->
#    "where" (WhereClauseItem ',')* WhereClauseItem?
    
# WhereClauseItem ->
#     Type ':' TypeParamBounds?
    
Struct ->
      StructStruct
   #| TupleStruct
    
StructStruct ->
    "struct" IDENTIFIER #GenericParams?# #WhereClause?#
    (‘{’ StructFields '}' | ';')
    
StructFields ->
    StructField (',' StructField)* ','?
    
StructField ->
    IDENTIFIER ':' Type
    
Enumeration ->
    "enum" IDENTIFIER #GenericParams?# #WhereClause?#
     '{' EnumItems? '}'
    
EnumItems ->
    EnumItem (',' EnumItem)* ','?

# No more grammar sugar
EnumItem -> 
    IDENTIFIER #(EnumItemTuple | EnumItemStruct)#
    EnumItemDiscriminant?

# EnumItemTuple -> ...

# EnumItemStruct -> ...

# Expression here shall be constant
EnumItemDiscriminant ->
    '=' Expression

# Expression here shall be constant
ConstantItem ->
    "const" (IDENTIFIER | '_') ':' Type ('=' Expression)? ';'

Trait ->
    "trait" IDENTIFIER #GenericParams?#
    #(':' TypeParamBounds?)?# #WhereClause?#
    '{' AssociatedItem* '}'

AssociatedItem ->
      AssociatedTypeAlias 
    | AssociatedConstantItem 
    | AssociatedFunction

AssociatedTypeAlias ->
    TypeAlias
    
AssociatedConstantItem ->
    ConstantItem

AssociatedFunction ->
    Function

# Simplified version
TypeAlias ->
    "type" IDENTIFIER #GenericParams# #':' TypeParamBounds?#
    #WhereClause?#
    ('=' Type #WhereClause?#)? ';'

Implementation ->
      InherentImpl 
    | TraitImpl

InherentImpl ->
    "impl" #GenericParams?# Type #WhereClause?#
    '{' AssociatedItem* '}'
    
# deleted negative implementation sign due to not supplying unsafe properties
TraitImpl ->
    "impl" #GenericParams?# #'!'?# TypePath "for" Type #WhereClause?#
    '{' AssociatedItem* '}'

# Simplified: No closures / generics, no mods / use crates
TypePath ->
    #"::"?# TypePathSegment ("::" TypePathSegment)*

# No Generic Args
TypePathSegment -> 
    PathIdentSegment #("::"? TypePathFn)?#

# Not every possibility can be used    
PathIdentSegment ->
    IDENTIFIER | "super" | "self" | "Self" | "crate" #| "$crate"#

# TypePathFn ->
#     '(' TypePathFnInputs? ')' ("->" TypeNoBounds)?

# TypePathFnInputs -> 
#     Type (',' Type)* ','?

```
---
Take a breath.
Expression part
```
Expression ->
      ExpressionWithoutBlock
    | ExpressionWithBlock
    
ExpressionWithoutBlock ->
      LiteralExpression         # termination. Remember to record the basic types.
    | PathExpression            # Field::Field... no generics temporarily
    | OperatorExpression        # operator+-*/%=...
    | GroupedExpression         # (Expr) in priority parentheses.
    | ArrayExpression           # T[N] / {t1, t2, t3, ...}
    | IndexExpression           # operator[]
    | TupleExpression           # {a1, b2, c3, ...}. Don't forget unit type "()".
    | TupleIndexingExpression   # tuple.get<N>
    | StructExpression          # Struct constructor
    | CallExpression            # func() | Normal function call
    | MethodCallExpression      # obj.method() | Method function call
    | FieldExpression           # obj.field | Field reference
    | ContinueExpression        # "continue"
    | BreakExpression           # "break"
    | RangeExpression           # "a..b"/"a..=b"
    | ReturnExpression          # "return"
    | UnderscoreExpression      # "_" discarded values
    
LiteralExpression ->
      CHAR_LITERAL
    | STRING_LITERAL
    | INTEGER_LITERAL
    | FLOAT_LITERAL
    | "true"
    | "false"

PathExpression ->
      PathInExpression
   #| QualifiedPathInExpression
    
PathInExpression ->
    "::"? PathExprSegment ("::" PathExprSegment)*
    
PathExprSegment ->
    PathIdentSegment #("::" GenericArgs)?#
    
OperatorExpression ->
      BorrowExpression
    | DereferenceExpression
    | NegationExpression
    | ArithmeticOrLogicalExpression
    | ComparisonExpression
    | LazyBooleanExpression
    | TypeCastExpression
    | AssignmentExpression
    | CompoundAssignmentExpression

BorrowExpression ->
    ('&' | "&&") "mut"? Expression
    
DereferenceExpression ->
    '*' Expression

NegationExpression ->
    ('-' | '!') Expression

ArithmeticOrLogicalExpression ->
    Expression ('+' | '-' | '*' | '/' | '%' | '&' | '|' | '^' | "<<" | ">>") Expression
    
ComparisonExpression ->
    Expression ("==" | "!=" | '>' | '<' | ">=" | "<=") Expression
    
LazyBooleanExpression ->
    Expression ("||" | "&&") Expression
      
TypeCastExpression ->
    Expression "as" TypeNoBounds
    
AssignmentExpression ->
    Expression '=' Expression

CompoundAssignmentExpression ->
    Expression ("+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | "<<=" | ">>=") Expression

GroupedExpression ->
    '(' Expression ')'

ArrayExpression ->
    '[' ArrayElements? ']'

ArrayElements →
      ExplicitArrayElements
    | RepeatedArrayElements

ExplicitArrayElements ->
    Expression (',' Expression)* ','?

RepeatedArrayElements ->
    Expression ; Expression

IndexExpression ->
    Expression '[' Expression ']'
    
TupleExpression ->
    '(' TupleElements? ')'

TupleElements ->
    (Expression ',')+ Expression?
    
TupleIndexingExpression ->
    Expression '.' TUPLE_INDEX
    
StructExpression ->
    PathInExpression '{' (StructExprFields | #StructBase#)? '}'

StructExprFields ->
    StructExprField (',' StructExprField)* ','? #(',' StructBase | ','?)#

# Identifier only is not supported now.
StructExprField ->
    (IDENTIFIER | TUPLE_INDEX) ':' Expression

NamedStructExprField ->
    IDENTIFIER ':' Expression
    
IndexStructExprField ->
    TUPLE_INDEX ':' Expression

# StructBase ->
#    ".." Expression
    
CallExpression ->
    Expression '( CallParams? ')'

CallParams ->
    Expression (',' Expression)* ','?

MethodCallExpression ->
    Expression '.' PathExprSegment '(' CallParams? ')'

FieldExpression ->
    Expression '.' IDENTIFIER

ContinueExpression ->
    "continue"
    
BreakExpression ->
    "break" Expression?
    
RangeExpression ->
      RangeExpr
    | RangeFromExpr
    | RangeToExpr
    | RangeFullExpr
    | RangeInclusiveExpr
    | RangeToInclusiveExpr

RangeExpr -> 
    Expression ".." Expression

RangeFromExpr ->
    Expression ".."

RangeToExpr ->
    ".." Expression

RangeFullExpr ->
    ".."

RangeInclusiveExpr ->
    Expression "..=" Expression

RangeToInclusiveExpr ->
    "..=" Expression
    
ReturnExpression ->
    "return" Expression?

UnderscoreExpression ->
    '_'

ExpressionWithBlock ->
      BlockExpression           # '{' something '}'
   #| ConstBlockExpression        "const" + BlockExpression. I defy it.
    | LoopExpression            # "loop", "while", "for"
    | IfExpression              # "if" "else"
    | MatchExpression           # "match" ... implement later

BlockExpression -> '{' Statements? '}'

# I modified it.
# Statements ->
#      Statement+
#    | ExpressionWithoutBlock
#    | Statement+ ExpressionWithoutBlock

Statements ->
    Statement* ExpressionWithoutBlock?

Statement ->
      EmptyStatement
    | ItemStatement
    | LetStatement
    | ExpressionStatement

ItemStatement ->
    Item

EmptyStatement ->
    ';'
    
# No bind-failure else expression
LetStatement ->
    "let" PatternNoTopAlt (':' Type)?
    ('=' Expression)? 

# The ast nodes will just use ExprStatement -> Expression.
# The semi problem will be handled in parser.
ExpressionStatement ->
      ExpressionWithoutBlock ';'
    | ExpressionWithBlock ';'?

LoopExpression ->
      InfiniteLoopExpression
    | PredicateLoopExpression
   #| IteratorLoopExpression

InfiniteLoopExpression ->
    "loop" BlockExpression

PredicateLoopExpression ->
    "while" Conditions BlockExpression
    
# IteratorLoopExpression ->
#     "for" Pattern "in" Expression #except StructExpression#
#      BlockExpression

IfExpression ->
    "if" Conditions BlockExpression
    ("else": (BlockExpression | IfExpression))?
    
# No LetChain
Conditions ->
    Expression #except StructExpression#

MatchExpression ->
    "match" Expression #expect StructExpression#
    '{' MatchArms? '}'

MatchArms ->
    (MatchArm "=>" (ExpressionWithoutBlock ',' | ExpressionWithBlock ','?))*
    MatchArm "=>" Expression ','?
    
MatchArm ->
    Pattern MatchArmGuard?

MatchArmGuard ->
    "if" Expression
    
Pattern ->
    '|'? PatternNoTopAlt ('|' PatternNoTopAlt)*

# No RangePattern
PatternNoTopAlt ->
      PatternWithoutRange
   #| RangePattern

PatternWithoutRange ->
      LiteralPattern
    | IdentifierPattern
    | WildcardPattern
   #| RestPattern
    | ReferencePattern
    | StructPattern
    | TuplePattern
    | GroupedPattern
    | SlicePattern
    | PathPattern
    
LiteralPattern ->
    '-'? LiteralExpression

IdentifierPattern ->
    "ref"? "mut"? IDENTIFIER ('@' PatternNoTopAlt)?
    
WildcardPattern ->
    '_'
    
# RestPattern ->
#     ".."
    
ReferencePattern ->
    ('&' || "&&") "mut"? PatternWithoutRange

StructPattern ->
    PathInExpression '{' StructPatternElements? '}'

StructPatternElements -> 
      StructPatternFields (',' #| ',' StructPatternEtCetera#)?
   #| StructPatternEtCetera
        
# StructPatternEtCetera ->
#     ".."

StructPatternFields ->
    StructPatternField (',' StructPatternField)*

# Uh... I removed some possibilities.
# You know, even the entire struct pattern is not being tested (for now).
StructPatternField ->
   #  TUPLE_INDEX ':' Pattern
    | IDENTIFIER ':' Pattern
   #| "ref"? "mut"? IDENTIFIER

IndexStructPatternField ->
    TUPLE_INDEX ':' Pattern

NamedStr

TuplePattern ->
    '(' TuplePatternItems? ')'
    
TuplePatternItems ->
      Pattern ','
   #| RestPattern
    | Pattern (',' Pattern)+ ','?

GroupedPattern ->
    '(' Pattern ')'

SlicePattern ->
    '[' SlicePatternItems? ']'

SlicePatternItems ->
    Pattern (',' Pattern)* ','?

PathPattern ->
    PathExpression

```