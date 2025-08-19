// With much reference to clang::RecursiveASTVisitor.
#ifndef INSOMNIA_AST_VISITOR_H
#define INSOMNIA_AST_VISITOR_H

#include "ast_fwd.h"

#define ISM_RS_VISIT_METHOD(Node) \
  virtual void visit(Node &) = 0;

namespace insomnia::rust_shard::ast {
class BasicVisitor {
public:
  virtual ~BasicVisitor() = default;
  INSOMNIA_RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_VISIT_METHOD)
};

}

#undef ISM_RS_VISIT_METHOD

#endif // INSOMNIA_AST_VISITOR_H
