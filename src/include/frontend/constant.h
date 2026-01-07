#ifndef RUST_SHARD_FRONTEND_CONSTANT_H
#define RUST_SHARD_FRONTEND_CONSTANT_H

#include <concepts>
#include <optional>
#include <variant>

#include "type.h"
#include "utils.h"


namespace insomnia::rust_shard::sconst {

struct ConstPrime;
struct ConstRef;
struct ConstRange;
struct ConstTuple;
struct ConstArray;
struct ConstStruct;
struct ConstSlice;

class ConstValue;

// contains std::monostate
using const_val_list = utils::type_list<
  std::monostate,
  ConstPrime,
  ConstRef,
  ConstRange,
  ConstTuple,
  ConstArray,
  ConstStruct,
  ConstSlice
>;

class ConstValPtr {
  std::shared_ptr<ConstValue> _ptr;
public:
  ConstValPtr() = default;
  explicit ConstValPtr(std::shared_ptr<ConstValue> ptr): _ptr(std::move(ptr)) {}
  ConstValPtr(const ConstValPtr &other) = default;
  ConstValPtr(ConstValPtr &&) noexcept = default;
  ConstValPtr& operator=(const ConstValPtr &) = default;
  ConstValPtr& operator=(ConstValPtr &&) noexcept = default;
  ~ConstValPtr() = default;
  explicit operator bool() const { return static_cast<bool>(_ptr); }
  bool operator==(const ConstValPtr &other) const;

  const ConstValue& operator*() const { return *_ptr; }
  const ConstValue* operator->() const { return _ptr.get(); }
  ConstValue& operator*() { return *_ptr; }
  ConstValue* operator->() { return _ptr.get(); }
};

struct ConstBase {
  bool operator==(const ConstBase &) const = default;
  ~ConstBase() = default;
protected:
  ConstBase() = default; // hides outer construction
};
struct ConstPrime : public ConstBase {
  stype::TypePrime prime;
  utils::primitive_variant value;

  template <class T> requires utils::is_primitive<T>
  ConstPrime(stype::TypePrime _prime, T &&spec): prime(_prime), value(std::forward<T>(spec)) {}
  bool operator==(const ConstPrime&) const = default;
  // extract iXX and uXX as usize. can be used in multiple places.
  std::optional<stype::usize_t> get_usize() const { // NOLINT
    return std::visit([&]<typename T0>(T0 &&arg) {
      using T = std::decay_t<T0>;
      if constexpr(std::is_same_v<T, std::int64_t>) {
        if(arg > 0) return std::make_optional(static_cast<stype::usize_t>(arg));
      } else if constexpr(std::is_same_v<T, std::uint64_t>) {
        return std::make_optional(static_cast<stype::usize_t>(arg));
      }
      return std::optional<stype::usize_t>(std::nullopt);
    }, value);
  }
  std::optional<stype::isize_t> get_integer() const {
    return std::visit([&]<typename T0>(T0 &&arg) {
      using T = std::decay_t<T0>;
      if constexpr(utils::is_one_of<T, std::int64_t, std::uint64_t>) {
        return std::make_optional(static_cast<stype::isize_t>(arg));
      }
      return std::optional<stype::isize_t>(std::nullopt);
    }, value);
  }
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

  explicit ConstArray(std::vector<ConstValPtr> &&arr)
  : array(std::move(arr)) {}
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
  std::unordered_map<StringT, ConstValPtr> fields;

  ConstStruct(std::unordered_map<StringT, ConstValPtr> &&f): fields(std::move(f)) {}
  bool operator==(const ConstStruct &) const = default;
};
struct ConstRef : public ConstBase {
  ConstValPtr inner;
  bool ref_is_mut;
  ConstRef(ConstValPtr r, bool _ref_is_mut): inner(std::move(r)), ref_is_mut(_ref_is_mut) {}
  bool operator==(const ConstRef &) const = default;
};

class ConstValue {
public:
  bool operator==(const ConstValue &) const = default;
  template <class T> requires const_val_list::contains<T>
  ConstValue(stype::TypePtr type, T &&value)
  : _type(std::move(type)), _const_val(std::forward<T>(value)) {}
  template <class T> requires const_val_list::contains<T>
  void set(stype::TypePtr type, T &&value) {
    _type = std::move(type); _const_val = std::forward<T>(value);
  }
  void set_type(stype::TypePtr type) {
    _type = std::move(type);
  }
  template <class T> requires const_val_list::contains<T>
  const T& get() const;
  template <class T> requires const_val_list::contains<T>
  const T* get_if() const;

private:
  stype::TypePtr _type;
  // add a std::monostate to signal for being not constant
  const_val_list::prepend<std::monostate>::as_variant _const_val;
public:
  stype::TypePtr type() const { return _type; }
  stype::TypeKind kind() const { return _type->kind(); }
  const decltype(_const_val)& const_val() const { return _const_val; }
};

class ConstPool {
public:
  ConstPool() = default;
  // give the semantic type by the user.
  template <class T, class... Args> requires
     std::derived_from<T, ConstBase> &&
     std::is_constructible_v<T, Args...>
  std::shared_ptr<ConstValue> make_raw_const(stype::TypePtr type, Args &&...args) {
    auto ptr = std::make_shared<ConstValue>(
      std::move(type),
      T(std::forward<Args>(args)...)
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
  ConstValPtr make_const(stype::TypePtr type, Args &&...args) {
    return ConstValPtr(make_raw_const<T>(std::move(type), std::forward<Args>(args)...));
  }
private:
  struct ConstValueSharedPtrHash {
    std::size_t operator()(const std::shared_ptr<ConstValue> &ptr) const;
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

#include "constant.ipp"

#endif // RUST_SHARD_FRONTEND_CONSTANT_H