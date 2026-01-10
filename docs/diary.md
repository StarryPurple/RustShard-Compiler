expr12: If condition must have parentheses.

if4: StringLiterals are &str, not str.

basic4: len() and length() exists together.

return10: ifExpr with no elseExpr shall be seen as having a elseExpr that returns ().

return13: Return statement can be used as expression value (never type coercion)... Make them happy.

misc57: Overflow in LiteralExpression. The fault is not in line 91, but in line 133 ~ 135.

return5/return6/return7: more main/exit behaviors (I'd like to take them as warnings...).

comprehensive (2,19,34,37,...): Allow a strong variable shadowing.

comprehensive 29: !i32 is allowed.
(line 267   registers[r_dst as usize] = !registers[r_dst as usize];)



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
  / expressions used by borrow expression
as address-needed.
(maybe not in semantic check phase) and reduce some useless store-n-load pairs.

The addr-needed and lside property shall not be seen as one.

In-place construction optimization (explicit array / field) is needed.

Unimplemented: It is said that in Rust
  let mut pool = [Node { edges: [f(); 201] }; 100];
will call f() for 20100 times. Hope no testcases cover it.

comprehensive 3: (fibonacci function)
If you simply move pointers in let statements (like "a->this = &rhs"), be careful that
the rhs ptr shall be a rvalue ptr (otherwise you'll meet bindings).
You can use "RegValue visit(node)" pattern to more precisely identify whether expressions returns lvalues.
(Assignments and compound assignments need rhs to be value, so it can't be optimized.)
(Let statements binds pointers, so we have this optimization.)

comprehensive 38:
At normal cases like (let x; { let x; }), simply eliminating symbol "x" when exiting scope will cause problem.
Also in definition this also causes problems: If building one more, this may conflict with variable shadowing (let x: t1; let x: t2)
One solution is to record the reg-map in Scope (so heavy) or a scope-like reg-map stack.

comprehensive 50: printlnInt -> printInt

comprehensive 15, 21: require sret(RVO)...
21 contains tail StructExpr and PathInExpr. 

comprehensive 27: be aware that 0x10 is 16, not 0.
std::stoll("0x10") is 0, but you can use std::stoll("0x10", nullptr, 0) to automatically check the base prefix.
std::stoull is the same.

comprehensive 19: the first 150 shall be 250 (comment in rx file say it should be 50... hmm.)

comprehensive 6/7/16/17/18...? 
optimization: integer literal constant integrated into BinaryOperations as instant value without alloca.
That saves stack space. Or use "add 0 val" (+ store) instead of alloca + store (+ load)

comprehensive 15(?): CallExpr and MethodCallExpr cannot be lvalue.