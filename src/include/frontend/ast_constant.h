#ifndef INSOMNIA_AST_CONSTANT_H
#define INSOMNIA_AST_CONSTANT_H

#include <concepts>
#include <optional>
#include <variant>

#include "ast_type.h"
#include "type_utils.h"


namespace insomnia::rust_shard::sem_const {

class ConstValue;

class ConstValPtr {
  std::shared_ptr<ConstValue> _ptr;
public:
  ConstValPtr() = default;
  ConstValPtr(std::shared_ptr<ConstValue> ptr): _ptr(std::move(ptr)) {}
  ConstValPtr(const ConstValPtr &other) = default;
  ConstValPtr(ConstValPtr &&) noexcept = default;
  ConstValPtr& operator=(const ConstValPtr &) = default;
  ConstValPtr& operator=(ConstValPtr &&) noexcept = default;
  ~ConstValPtr() = default;
  explicit operator bool() const { return static_cast<bool>(_ptr); }
  bool operator==(const ConstValPtr &other) const;

  const ConstValue& operator*() const { return *_ptr; }
  const ConstValue* operator->() const { return _ptr.get(); }
};

struct ConstBase {
  bool operator==(const ConstBase &) const = default;
  ~ConstBase() = default;
protected:
  ConstBase() = default; // hides outer construction
};
struct ConstPrimitive : public ConstBase {
  // The underlying type is determined by "type" in ConstValue wrapper
  type_utils::primitive_variant value;
  template <class T> requires type_utils::is_primitive<T>
  explicit ConstPrimitive(T &&spec): value(std::forward<T>(spec)) {}
  bool operator==(const ConstPrimitive&) const = default;
};
struct ConstRange : public ConstBase {
  std::optional<ConstValPtr> begin, end;
  bool operator==(const ConstRange &) const = default;
};
struct ConstTuple : public ConstBase {
  std::vector<ConstValPtr> tuple;
  bool operator==(const ConstTuple &) const = default;
};
struct ConstArray : public ConstBase {
  std::vector<ConstValPtr> array;
  std::size_t length() const { return array.size(); }
  bool operator==(const ConstArray &) const = default;
};
struct ConstSlice : public ConstBase {
  std::shared_ptr<ConstArray> array;
  std::size_t begin, length;

  ConstSlice(std::shared_ptr<ConstArray> &&a, std::size_t beg, std::size_t len)
  : array(std::move(a)), begin(beg), length(len) {}
  bool operator==(const ConstSlice &) const = default;
};
struct ConstStruct : public ConstBase {
  std::unordered_map<std::string_view, ConstValPtr> fields;

  ConstStruct(std::unordered_map<std::string_view, ConstValPtr> &&f): fields(std::move(f)) {}
  bool operator==(const ConstStruct &) const = default;
};
struct ConstReference : public ConstBase {
  ConstValPtr ref;
  ConstReference(ConstValPtr r): ref(std::move(r)) {}
  bool operator==(const ConstReference &) const = default;
};

class ConstValue {
public:
  bool operator==(const ConstValue &) const = default;
  template <class T>
  ConstValue(sem_type::TypePtr type, const T &value) {
    set(std::move(type), value);
  }
  template <class T>
  const T& get() const;
  template <class T>
  const T* get_if() const;
  template <class T>
  void set(sem_type::TypePtr type, const T &value);

private:
  sem_type::TypePtr _type;
  std::variant<
    std::monostate,
    ConstPrimitive,
    ConstReference,
    ConstRange,
    ConstTuple,
    ConstArray,
    ConstStruct,
    ConstSlice
  > _const_val;
public:
  sem_type::TypePtr type() const { return _type; }
  sem_type::TypeKind kind() const { return _type->kind(); }
  const decltype(_const_val)& const_val() const { return _const_val; }
};

class ConstPool {
public:
  ConstPool() = default;
  // give the semantic type by the user.
  template <class T, class... Args> requires
     std::derived_from<T, ConstBase> &&
     std::is_constructible_v<T, Args...>
  std::shared_ptr<ConstValue> make_raw_const(sem_type::TypePtr type, Args &&...args) {
    auto ptr = std::make_shared<ConstValue>(
      std::move(type),
      std::make_shared<T>(std::forward<Args>(args)...)
    );
    auto it = _pool.find(ptr);
    if(it != _pool.end())
      return std::static_pointer_cast<ConstValue>(*it); // logically confirmed
    _pool.insert(ptr);
    return ptr;
  }
  template <class T, class... Args> requires
     std::derived_from<T, ConstBase> &&
     std::is_constructible_v<T, Args...>
  ConstValPtr make_const(sem_type::TypePtr type, Args &&...args) {
    return ConstValPtr(make_raw_const<T>(std::move(type), std::forward<Args>(args)...));
  }
private:
  struct ConstValueSharedPtrHash {
    std::size_t operator()(const std::shared_ptr<ConstValue> &ptr) const {
      std::size_t type_hash = ptr->type()->hash();
      std::size_t value_hash = 0;
      std::visit([&]<typename T0>(const T0 &val) {
        using T = std::decay_t<T0>;
        if constexpr(std::is_same_v<T, ConstPrimitive>) {
          std::visit([&]<typename T1>(const T1& primitive_val) {
            value_hash = std::hash<std::decay_t<T1>>()(primitive_val);
          }, val.value);
        } else if constexpr(!std::is_same_v<T, std::monostate>) {
          // value_hash = std::hash<T>()(val);
        }
      }, ptr->const_val());
      return type_hash ^ value_hash;
    }
  };
  struct ConstValueSharedPtrEqual {
    bool operator()(
      const std::shared_ptr<ConstValue> &A,
      const std::shared_ptr<ConstValue> &B
    ) const {
      return *A->type() == *B->type() && A->const_val() == B->const_val();
    }
  };
  std::unordered_set<
    std::shared_ptr<ConstValue>,
    ConstValueSharedPtrHash,
    ConstValueSharedPtrEqual
  > _pool;
};

}

#endif // INSOMNIA_AST_CONSTANT_H