expr12: If condition must have parentheses.

if4: StringLiterals are &str, not str.

basic4: len() and length() exists together.

return10: ifExpr with no elseExpr shall be seen as having a elseExpr that returns ().

return13: Return statement can be used as expression value (never type coercion)... Make them happy.

misc57: Overflow in LiteralExpression. The fault is not in line 91, but in line 133 ~ 135.

return5/return6/return7: more main/exit behaviors (I'd like to take them as warnings...).

comprehensive (2,19,34,37,...): Allow a strong variable shadowing.

comprehensive 29: !i32 is allowed.



comprehensive 46: ItemStatement allows functions defined inside functions.

CompoundAssignment shall return ().

LLVM integer types all start with 'i', no unsigned.

logicalAnd/logicalOr won't appear in ArithmeticOrLogicalExpression...

You don't need to recursively infilct is_lside property... only one layer is sufficient.

Flaw in control flow tags... when writing phi nodes, use current tags rather than the original then/else 
(they might end early due to other control flow expressions)

IRGenerator: lifetimes of variables follow scopes, not functions.

The stack space is not that unlimited (especially when you need to create a large array).
Tag the right side of let statement
  / expressions of fields in struct construction expression
  / expressions of entries in array construction expression
as address-needed.
(maybe not in semantic check phase) and reduce some useless store-n-load pairs.

The addr-needed and lside property shall not be seen as one.

In-place construction optimization (explicit array / field) is needed.

Unimplemented: It is said that in Rust
  let mut pool = [Node { edges: [f(); 201] }; 100];
will call f() for 20100 times. Hope no testcases cover it.