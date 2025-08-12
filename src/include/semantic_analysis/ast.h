#ifndef INSOMNIA_AST_H
#define INSOMNIA_AST_H

#include <memory>
#include <vector>

namespace insomnia {

class Visitor;

class ASTNode {
public:
  virtual ~ASTNode() = default;
  virtual void accept(Visitor &visitor) = 0;
};

class ASTFunction : public ASTNode {

};

class ASTStruct : public ASTNode {

};

class ASTEnumeration : public ASTNode {

};

class ASTConstantItem : public ASTNode {

};

class ASTImplementation : public ASTNode {

};

// VisItem -> Function | Struct | Enumeration | ConstantItem | Trait | Implementation
class ASTVisItem : public ASTNode {

};

// Item -> VisItem
class ASTItem : public ASTNode {

};

// Crate -> Item*
class ASTCrate : public ASTNode {
  ASTCrate(std::vector<std::unique_ptr<ASTItem>> items) : _items(std::move(items)) {}

  void accept(Visitor &visitor) override;

private:
  std::vector<std::unique_ptr<ASTItem>> _items;
};

class Visitor {
public:
  virtual ~Visitor() = default;
  virtual void visit(ASTVisItem &) = 0;
  virtual void visit(ASTItem &) = 0;
  virtual void visit(ASTCrate &) = 0;
};

}

#endif // INSOMNIA_AST_H