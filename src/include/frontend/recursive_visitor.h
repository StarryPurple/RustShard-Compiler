#ifndef RUST_SHARD_FRONTEND_RECURSIVE_VISITOR_H
#define RUST_SHARD_FRONTEND_RECURSIVE_VISITOR_H

#include "visitor.h"

#define ISM_RS_PRE_VISIT_METHOD(Node) \
  virtual void preVisit(Node &node) {}
#define ISM_RS_POST_VISIT_METHOD(Node) \
  virtual void postVisit(Node &node) {}
#define ISM_RS_VISIT_METHOD(Node) \
  void visit(Node &node) override;

namespace insomnia::rust_shard::ast {

class RecursiveVisitor : public BasicVisitor {
public:
  void traverse(Crate &crate);
protected:
  // better call "traverse" here rather than "visit"

  RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_VISIT_METHOD)
  RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_PRE_VISIT_METHOD)
  RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_POST_VISIT_METHOD)
};

}

#undef ISM_RS_PRE_VISIT_METHOD
#undef ISM_RS_POST_VISIT_METHOD
#undef ISM_RS_VISIT_METHOD

#endif // RUST_SHARD_FRONTEND_RECURSIVE_VISITOR_H