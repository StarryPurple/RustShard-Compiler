#ifndef INSOMNIA_AST_H
#define INSOMNIA_AST_H

#include <memory>
#include <variant>
#include <vector>

namespace insomnia {

class Visitor;

class ASTNode {
public:
  virtual ~ASTNode() = default;
  virtual void accept(Visitor &visitor) = 0;
};

// "const"? "fn" IDENTIFIER GenericParams? '(' FunctionParameters? ')'
// (-> Type)? WhereClause? (BlockExpression | ;)
class ASTFunction : public ASTNode {

};

class ASTStruct : public ASTNode {

};

class ASTEnumeration : public ASTNode {

};

class ASTConstantItem : public ASTNode {

};

class ASTTrait : public ASTNode {

};

class ASTImplementation : public ASTNode {

};

// VisItem -> Function | Struct | Enumeration | ConstantItem | Trait | Implementation
class ASTVisItem : public ASTNode {
public:

  template <class T>
  ASTVisItem(T &&spec_item) : _spec_item(std::forward<T>(spec_item)) {}

  void accept(Visitor &visitor) override {
    std::visit([&](auto &item) { item->accept(visitor); }, _spec_item);
  }

private:
  std::variant<
    std::unique_ptr<ASTFunction>,
    std::unique_ptr<ASTStruct>,
    std::shared_ptr<ASTEnumeration>,
    std::shared_ptr<ASTConstantItem>,
    std::shared_ptr<ASTTrait>,
    std::shared_ptr<ASTImplementation>
  > _spec_item;
};

// Item -> VisItem
class ASTItem : public ASTNode {
public:
  template <class T>
  ASTItem(T &&vis_item) : _vis_item(std::forward<T>(vis_item)) {}

  void accept(Visitor &visitor) override {
    _vis_item->accept(visitor);
  }

private:
  std::unique_ptr<ASTVisItem> _vis_item;
};

// Crate -> Item*
class ASTCrate : public ASTNode {
public:
  template <class T>
  ASTCrate(T &&items) : _items(std::forward<T>(items)) {}

  void accept(Visitor &visitor) override {
    for(auto &item : _items)
      item->accept(visitor);
  }

private:
  std::vector<std::unique_ptr<ASTItem>> _items;
};

class Visitor {
public:
  virtual ~Visitor() = default;
  virtual void visit(ASTFunction &) = 0;
  virtual void visit(ASTStruct &) = 0;
  virtual void visit(ASTEnumeration &) = 0;
  virtual void visit(ASTConstantItem &) = 0;
  virtual void visit(ASTImplementation &) = 0;
  virtual void visit(ASTVisItem &) = 0;
  virtual void visit(ASTItem &) = 0;
  virtual void visit(ASTCrate &) = 0;
};

}

#endif // INSOMNIA_AST_H