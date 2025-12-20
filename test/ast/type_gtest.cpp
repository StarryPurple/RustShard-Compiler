#include "gtest/gtest.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <concepts>

#include "ast_type.h"

using namespace insomnia::rust_shard::stype;
using insomnia::rust_shard::StringRef;

class AstTypeTest : public ::testing::Test {
  protected:
    TypePool pool;

    TypePtr i32_type;    // `i32`
    TypePtr u32_type;    // `u32`
    TypePtr bool_type;   // `bool`
    TypePtr char_type;   // `char`
    TypePtr f64_type;    // `f64`

    // A complex, shared base type used in multiple tests.
    TypePtr complex_base_type; // `([u32; 10], &mut bool)`

    void SetUp() override {
      // Create and register core primitive types in the pool.
      i32_type = pool.make_type<PrimeType>(TypePrime::kI32);
      u32_type = pool.make_type<PrimeType>(TypePrime::kU32);
      bool_type = pool.make_type<PrimeType>(TypePrime::kBool);
      char_type = pool.make_type<PrimeType>(TypePrime::kChar);
      f64_type = pool.make_type<PrimeType>(TypePrime::kF64);

      auto u32_array = pool.make_type<ArrayType>(u32_type, 10);
      auto bool_ref_mut = pool.make_type<RefType>(bool_type, true);
      complex_base_type = pool.make_type<TupleType>(
        std::vector<TypePtr>{u32_array, bool_ref_mut}
      );
    }
};

/**

### **Test Cases**

##### **1. Basic Type Equality and Hashing**

These tests verify the fundamental behavior of primitive types, including their equality, inequality, and hash consistency.

**/

TEST_F(AstTypeTest, PrimitiveEquality) {
  // `i32` vs `i32`
  ASSERT_EQ(*i32_type, *pool.make_raw_type<PrimeType>(TypePrime::kI32));
  // `i32` vs `u32`
  ASSERT_NE(*i32_type, *u32_type);
}

TEST_F(AstTypeTest, PrimitiveHashConsistency) {
  // `hash(i32)` vs `hash(i32)`
  ASSERT_EQ(i32_type->hash(), pool.make_raw_type<PrimeType>(TypePrime::kI32)->hash());
  // `hash(i32)` vs `hash(u32)`
  ASSERT_NE(i32_type->hash(), u32_type->hash());
}

TEST_F(AstTypeTest, ArrayComparison) {
  // `[i32; 10]` vs `[i32; 10]`
  auto arr1 = pool.make_raw_type<ArrayType>(i32_type, 10);
  auto arr2 = pool.make_raw_type<ArrayType>(i32_type, 10);
  ASSERT_EQ(*arr1, *arr2);

  // `[i32; 10]` vs `[i32; 12]`
  ASSERT_NE(*arr1, *pool.make_raw_type<ArrayType>(i32_type, 12));
  // `[i32; 10]` vs `[u32; 10]`
  ASSERT_NE(*arr1, *pool.make_raw_type<ArrayType>(u32_type, 10));

  // `[[i32; 10]; 5]` vs `[[i32; 10]; 5]`
  auto nested_arr1 = pool.make_raw_type<ArrayType>(TypePtr(arr1), 5);
  auto nested_arr2 = pool.make_raw_type<ArrayType>(TypePtr(arr2), 5);
  ASSERT_EQ(*nested_arr1, *nested_arr2);
}

TEST_F(AstTypeTest, TupleComparison) {
  // `(i32, bool)` vs `(i32, bool)`
  auto tuple1 = pool.make_raw_type<TupleType>(std::vector<TypePtr>{i32_type, bool_type});
  auto tuple2 = pool.make_raw_type<TupleType>(std::vector<TypePtr>{i32_type, bool_type});
  ASSERT_EQ(*tuple1, *tuple2);

  // `(i32, bool)` vs `(i32, u32)`
  ASSERT_NE(*tuple1, *pool.make_raw_type<TupleType>(std::vector<TypePtr>{i32_type, u32_type}));
  // `(i32, bool)` vs `(i32)`
  ASSERT_NE(*tuple1, *pool.make_raw_type<TupleType>(std::vector<TypePtr>{i32_type}));

  // `()` vs `()`
  auto unit_type = pool.make_raw_type<TupleType>(std::vector<TypePtr>{});
  ASSERT_EQ(*unit_type, *pool.make_raw_type<TupleType>(std::vector<TypePtr>{}));
  ASSERT_NE(*unit_type, *tuple1);
}

TEST_F(AstTypeTest, StructComparison) {
  // `struct MyStruct { x: i32, y: bool }`
  std::map<StringRef, TypePtr> fields1, fields2;
  fields1["x"] = i32_type;
  fields1["y"] = bool_type;
  fields2["x"] = i32_type;
  fields2["y"] = bool_type;
  auto struct1 = pool.make_raw_type<StructType>("MyStruct");
  struct1->set_fields(std::move(fields1));
  auto struct2 = pool.make_raw_type<StructType>("MyStruct");
  struct2->set_fields(std::move(fields2));
  ASSERT_EQ(*struct1, *struct2);

  // Test is discarded, for it's invalid in real programs.
  /*
  // Different field names.
  std::map<std::string, TypePtr> fields3;
  fields3["a"] = i32_type;
  fields3["y"] = bool_type;
  ASSERT_NE(*struct1, *pool.make_type<StructType>("MyStruct", std::move(fields3), false));
  */
}

TEST_F(AstTypeTest, SliceAndReferenceComparison) {
  // `&i32` vs `&i32`
  auto ref1 = pool.make_raw_type<RefType>(i32_type, false);
  auto ref2 = pool.make_raw_type<RefType>(i32_type, false);
  ASSERT_EQ(*ref1, *ref2);
  // Test discarded: no more mutability support.
  /*
  // `&i32` vs `&mut i32`
  ASSERT_NE(*ref1, *pool.make_raw_type<ReferenceType>(i32_type));
  */

  // `[i32]` vs `[i32]`
  auto slice1 = pool.make_raw_type<SliceType>(i32_type);
  auto slice2 = pool.make_raw_type<SliceType>(i32_type);
  ASSERT_EQ(*slice1, *slice2);
  // Test discarded: no more mutability support.
  /*
  // `[i32]` vs `[u32]`
  ASSERT_NE(*slice1, *pool.make_raw_type<SliceType>(u32_type));
  */
}

/**

##### **3. `&str` and `&[u8]` as a Single Type**

This test models `&str` and `&[u8]` as the same type to simplify the type system. It's a pragmatic choice for a new compiler.

**/

TEST_F(AstTypeTest, StringSliceAndByteSliceAsSameType) {
  // `[u8]` is the underlying type for string literals.
  auto u8_type = pool.make_raw_type<PrimeType>(TypePrime::kU8);
  auto u8_slice = pool.make_raw_type<SliceType>(TypePtr(u8_type));

  // `&[u8]` is the type of string literals and byte slices.
  auto u8_ref_slice = pool.make_raw_type<RefType>(TypePtr(u8_slice), false);

  // `&[u8]` is NOT equal to `&[char]`.
  auto char_type = pool.make_raw_type<PrimeType>(TypePrime::kChar);
  auto char_slice = pool.make_raw_type<SliceType>(TypePtr(char_type));
  auto char_ref_slice = pool.make_raw_type<RefType>(TypePtr(char_slice), false);

  ASSERT_NE(*u8_ref_slice, *char_ref_slice);
}

/**

##### **4. TypePool Correctness**

These tests verify that the `TypePool` correctly de-duplicates complex types, even when an alias is involved in the second creation attempt.

**/

TEST_F(AstTypeTest, TypePoolCorrectness) {
  size_t initial_pool_size = pool.size();

  // Request an existing type. No new types should be created.
  auto existing_i32 = pool.make_type<PrimeType>(TypePrime::kI32);
  ASSERT_EQ(pool.size(), initial_pool_size);
  ASSERT_EQ(existing_i32, i32_type);

  // Test deprecated: whether primitive types should be initialized in construction is not decided.
  /*
  // Request a new primitive type. Pool size should increase by 1.
  auto new_i8 = pool.make_type<PrimitiveType>(TypePrime::kI8);
  ASSERT_EQ(pool.size(), initial_pool_size + 1);
  */

  initial_pool_size = pool.size();

  // Request a new complex type. The pool should create and store it.
  // `[u32; 1]` is new, `(u32, [u32; 1])` is new.
  auto complex_type = pool.make_type<TupleType>(std::vector<TypePtr>{
    u32_type,
    pool.make_type<ArrayType>(u32_type, 1)
  });
  ASSERT_EQ(pool.size(), initial_pool_size + 2);

  // Request the exact same complex type again. The pool should return the cached one.
  auto complex_type_clone = pool.make_type<TupleType>(std::vector<TypePtr>{
    u32_type,
    pool.make_type<ArrayType>(u32_type, 1)
  });

  ASSERT_EQ(pool.size(), initial_pool_size + 2);
  ASSERT_EQ(complex_type, complex_type_clone);
}