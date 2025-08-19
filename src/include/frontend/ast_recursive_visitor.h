#ifndef INSOMNIA_AST_RECURSIVE_VISITOR_H
#define INSOMNIA_AST_RECURSIVE_VISITOR_H

#include "ast_visitor.h"

#define ISM_RS_PRE_VISIT_METHOD(Node) \
  virtual void preVisit(Node &) {}
#define ISM_RS_POST_VISIT_METHOD(Node) \
  virtual void postVisit(Node &) {}
#define ISM_RS_VISIT_METHOD(Node) \
  void visit(Node &node) override;

namespace insomnia::rust_shard::ast {

class RecursiveVisitor : public BasicVisitor {
public:
  void traverse(Crate &crate);
protected:
  INSOMNIA_RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_PRE_VISIT_METHOD)
  INSOMNIA_RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_POST_VISIT_METHOD)
private:
  // better call "traverse" here rather than "visit"

  INSOMNIA_RUST_SHARD_AST_VISITABLE_NODES_LIST(ISM_RS_VISIT_METHOD)
};

}

#undef ISM_RS_PRE_VISIT_METHOD
#undef ISM_RS_POST_VISIT_METHOD
#undef ISM_RS_VISIT_METHOD

#endif // INSOMNIA_AST_RECURSIVE_VISITOR_H