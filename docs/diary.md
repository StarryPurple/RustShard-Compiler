expr12: If condition must have parentheses.

if4: StringLiterals are &str, not str.

basic4: len() and length() exists together.

return10: ifExpr with no elseExpr shall be seen as having a elseExpr that returns ().

return13: Return statement can be used as expression value (never type coercion)... Make them happy.

misc57: Overflow in LiteralExpression. The fault is not in line 91, but in line 133 ~ 135.

return5/return6/return7: more main/exit behaviors (I'd like to take them as warnings...).

comprehensive (2,19,34,37,...): Allow a strong variable shadowing.

comprehensive 29: !i32 is allowed.