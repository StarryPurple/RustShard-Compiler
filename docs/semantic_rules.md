Single file, no modules, no crates.
No generics/templates/where clauses, no lifetimes.

```

Crate -> 
    Item

Item -> 
    VisItem

VisItem ->
      Function
    | Struct
    | Enumeration
    | ConstantItem
    | Trait
    | Implementation

Function ->
    "const"? "fn" IDENTIFIER #GenericParams?#
    '(' FunctionParameters? ')'
    (-> Type)? #WhereClause?# (BlockExpression | ';')
    
# GenericParams -> ...

FunctionParameters ->
      SelfParam ','? 
    | (SelfParam ',')? FunctionParam (',' FunctionParam)* ','?
    
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
    
StructStruct ->
    "struct" IDENTIFIER #GenericParams?# #WhereClause?#
    (‘{’ StructFields '}' | ';')
    
StructFields ->
    Structfield (',' StructField)* ','?
    
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

EnumItemDiscriminant ->
    '=' Expression
    
ConstantItem ->
    "const" (IDENTIFIER | '_') ':' Type ('=' Expression)? ';'

Trait ->
    "trait" IDENTIFIER #GenericParams?#
    #(':' TypeParamBounds?)?# #WhereClause?#
    '{' AssociatedItem* '}'
    
# No type alias supported
AssociatedItem ->
    ConstantItem | Function

# TypeAlias -> ...

Implementation ->
    InherentImpl | TraitImpl

InherentImpl ->
    "impl" #GenericParams?# Type #WhereClause?#
    '{' AssociatedItem* '}'
    
TraitImpl ->
    "impl" #GenericParams?# '!'? TypePath "for" Type #WhereClause?#
    '{' AssociatedItem* '}'
    
TypePath ->
    "::"? TypePathSegment ("::" TypePathsegment)*

# No Generic Args
TypePathSegment -> 
    PathIdentSegment ("::"? TypePathFn)?

# Not every possibility can be used    
PathIdentSegment ->
    IDENTIFIER | "super" | "self" | "Self" | "crate" | "$crate"

TypePathFn ->
    '(' TypePathFnInputs? ')' ("->" TypeNoBounds)?

TypePathFnInputs -> 
    Type (',' Type)* ','?

BlockExpression ->
    '{' Statements? '}'
    
Statements ->
      Statement+
    | ExpressionWithoutBlock
    | Statement+ ExpressionWithoutBlock

Expression ->
      ExpressionWithoutBlock
    | ExpressionWithBlock
    
ExpressionWithoutBlock ->
    ...
    
ExpressionWithBlock ->
    ...
    
```