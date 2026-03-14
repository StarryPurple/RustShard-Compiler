// With much reference to clang::RecursiveASTVisitor.
#ifndef RUSTS_SHARD_FRONTEND_VISITOR_H
#define RUSTS_SHARD_FRONTEND_VISITOR_H

#include "fwd.h"

#define ISM_RS_VISIT_METHOD(Node) \
  virtual void visit(Node &) = 0;

namespace insomnia::rust_shard::ast {
class BasicVisitor {
public:
  virtual ~BasicVisitor() = default;
  RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_VISIT_METHOD)
};

}

#undef ISM_RS_VISIT_METHOD

#endif // RUSTS_SHARD_FRONTEND_VISITOR_H
