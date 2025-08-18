#include "gtest/gtest.h"
#include <map>
#include <string>
#include <vector>
#include <memory>

#include "ast_type.h"

using namespace insomnia::rust_shard::type;

class AstTypeTest : public ::testing::Test {
  protected:
    TypePool pool;

    std::shared_ptr<PrimitiveType> i32_type;    // `i32`
    std::shared_ptr<PrimitiveType> u32_type;    // `u32`
    std::shared_ptr<PrimitiveType> bool_type;   // `bool`
    std::shared_ptr<PrimitiveType> char_type;   // `char`
    std::shared_ptr<PrimitiveType> f64_type;    // `f64`
    std::shared_ptr<PrimitiveType> i32_mut_type; // `&mut i32`

    // A complex, shared base type used in multiple tests.
    std::shared_ptr<TupleType> complex_base_type; // `([u32; 10], &mut bool)`

    void SetUp() override {
      // Create and register core primitive types in the pool.
      i32_type = pool.make_type<PrimitiveType>(TypePrime::I32, false);
      u32_type = pool.make_type<PrimitiveType>(TypePrime::U32, false);
      bool_type = pool.make_type<PrimitiveType>(TypePrime::BOOL, false);
      char_type = pool.make_type<PrimitiveType>(TypePrime::CHAR, false);
      f64_type = pool.make_type<PrimitiveType>(TypePrime::F64, false);
      i32_mut_type = pool.make_type<PrimitiveType>(TypePrime::I32, true);

      auto u32_array = pool.make_type<ArrayType>(u32_type, 10, false);
      auto bool_ref_mut = pool.make_type<ReferenceType>(bool_type, true);
      complex_base_type = pool.make_type<TupleType>(
        std::vector<std::shared_ptr<ExprType>>{u32_array, bool_ref_mut}, false
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
  ASSERT_EQ(*i32_type, *pool.make_type<PrimitiveType>(TypePrime::I32, false));
  // `i32` vs `u32`
  ASSERT_NE(*i32_type, *u32_type);
  // `i32` vs `&mut i32`
  ASSERT_NE(*i32_type, *i32_mut_type);
}

TEST_F(AstTypeTest, PrimitiveHashConsistency) {
  // `hash(i32)` vs `hash(i32)`
  ASSERT_EQ(i32_type->hash(), pool.make_type<PrimitiveType>(TypePrime::I32, false)->hash());
  // `hash(i32)` vs `hash(u32)`
  ASSERT_NE(i32_type->hash(), u32_type->hash());
  // `hash(i32)` vs `hash(&mut i32)`
  ASSERT_NE(i32_type->hash(), i32_mut_type->hash());
}

TEST_F(AstTypeTest, ArrayComparison) {
  // `[i32; 10]` vs `[i32; 10]`
  auto arr1 = pool.make_type<ArrayType>(i32_type, 10, false);
  auto arr2 = pool.make_type<ArrayType>(i32_type, 10, false);
  ASSERT_EQ(*arr1, *arr2);

  // `[i32; 10]` vs `[i32; 12]`
  ASSERT_NE(*arr1, *pool.make_type<ArrayType>(i32_type, 12, false));
  // `[i32; 10]` vs `[u32; 10]`
  ASSERT_NE(*arr1, *pool.make_type<ArrayType>(u32_type, 10, false));

  // `[[i32; 10]; 5]` vs `[[i32; 10]; 5]`
  auto nested_arr1 = pool.make_type<ArrayType>(arr1, 5, false);
  auto nested_arr2 = pool.make_type<ArrayType>(arr2, 5, false);
  ASSERT_EQ(*nested_arr1, *nested_arr2);
}

TEST_F(AstTypeTest, TupleComparison) {
  // `(i32, bool)` vs `(i32, bool)`
  auto tuple1 = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{i32_type, bool_type}, false);
  auto tuple2 = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{i32_type, bool_type}, false);
  ASSERT_EQ(*tuple1, *tuple2);

  // `(i32, bool)` vs `(i32, u32)`
  ASSERT_NE(*tuple1, *pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{i32_type, u32_type}, false));
  // `(i32, bool)` vs `(i32)`
  ASSERT_NE(*tuple1, *pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{i32_type}, false));

  // `()` vs `()`
  auto unit_type = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{}, false);
  ASSERT_EQ(*unit_type, *pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{}, false));
  ASSERT_NE(*unit_type, *tuple1);
}

TEST_F(AstTypeTest, StructComparison) {
  // `struct MyStruct { x: i32, y: bool }`
  std::map<std::string, std::shared_ptr<ExprType>> fields1, fields2;
  fields1["x"] = i32_type;
  fields1["y"] = bool_type;
  fields2["x"] = i32_type;
  fields2["y"] = bool_type;
  auto struct1 = pool.make_type<StructType>("MyStruct", std::move(fields1), false);
  auto struct2 = pool.make_type<StructType>("MyStruct", std::move(fields2), false);
  ASSERT_EQ(*struct1, *struct2);

  // Different field names.
  std::map<std::string, std::shared_ptr<ExprType>> fields3;
  fields3["a"] = i32_type;
  fields3["y"] = bool_type;
  ASSERT_NE(*struct1, *pool.make_type<StructType>("MyStruct", std::move(fields3), false));
}

TEST_F(AstTypeTest, SliceAndReferenceComparison) {
  // `&i32` vs `&i32`
  auto ref1 = pool.make_type<ReferenceType>(i32_type, false);
  auto ref2 = pool.make_type<ReferenceType>(i32_type, false);
  ASSERT_EQ(*ref1, *ref2);
  // `&i32` vs `&mut i32`
  ASSERT_NE(*ref1, *pool.make_type<ReferenceType>(i32_type, true));

  // `[i32]` vs `[i32]`
  auto slice1 = pool.make_type<SliceType>(i32_type, false);
  auto slice2 = pool.make_type<SliceType>(i32_type, false);
  ASSERT_EQ(*slice1, *slice2);
  // `[i32]` vs `[u32]`
  ASSERT_NE(*slice1, *pool.make_type<SliceType>(u32_type, false));
}

/**

**3. `AliasType` and Deeply Nested Alias Resolution**

These tests are critical for verifying that `remove_alias()` correctly normalizes types for comparison and hashing, even with deeply nested aliases.

**/

TEST_F(AstTypeTest, AliasComparisonAndHashing) {
  // `MyInt` is `i32`
  auto alias_i32 = pool.make_type<AliasType>("MyInt", i32_type);
  ASSERT_EQ(*alias_i32, *i32_type);
  ASSERT_EQ(alias_i32->hash(), i32_type->hash());

  // `MyTuple` is `([u32; 10], &mut bool)`
  auto alias_tuple = pool.make_type<AliasType>("MyTuple", complex_base_type);
  ASSERT_EQ(*alias_tuple, *complex_base_type);
  ASSERT_EQ(alias_tuple->hash(), complex_base_type->hash());

  // A chain of aliases: `AliasA` -> `AliasB` -> `AliasC` -> `i32`
  auto alias_a = pool.make_type<AliasType>("AliasA", i32_type);
  auto alias_b = pool.make_type<AliasType>("AliasB", alias_a);
  auto alias_c = pool.make_type<AliasType>("AliasC", alias_b);
  ASSERT_EQ(*alias_c, *i32_type);
  ASSERT_EQ(alias_c->hash(), i32_type->hash());

  // A deeply nested type with an alias at the 4th level.
  // Type: `(bool, [([MyType]); 2]); 3)` where `MyType` is `[i32; 1]`
  auto level1 = pool.make_type<ArrayType>(i32_type, 1, false);
  auto alias_my_type = pool.make_type<AliasType>("MyType", level1);
  auto level2 = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{bool_type, alias_my_type}, false);
  auto level3 = pool.make_type<ArrayType>(level2, 2, false);
  auto level4 = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{level3}, false);

  // The equivalent type without the alias.
  auto level1_b = pool.make_type<ArrayType>(i32_type, 1, false);
  auto level2_b = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{bool_type, level1_b}, false);
  auto level3_b = pool.make_type<ArrayType>(level2_b, 2, false);
  auto level4_b = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{level3_b}, false);

  ASSERT_EQ(*level4, *level4_b);
  ASSERT_EQ(level4->hash(), level4_b->hash());
}

/**

##### **4. `&str` and `&[u8]` as a Single Type**

This test models `&str` and `&[u8]` as the same type to simplify the type system. It's a pragmatic choice for a new compiler.

**/

TEST_F(AstTypeTest, StringSliceAndByteSliceAsSameType) {
  // `[u8]` is the underlying type for string literals.
  auto u8_type = pool.make_type<PrimitiveType>(TypePrime::U8, false);
  auto u8_slice = pool.make_type<SliceType>(u8_type, false);

  // `&[u8]` is the type of string literals and byte slices.
  auto u8_ref_slice = pool.make_type<ReferenceType>(u8_slice, false);

  // `&[u8]` is NOT equal to `&[char]`.
  auto char_type = pool.make_type<PrimitiveType>(TypePrime::CHAR, false);
  auto char_slice = pool.make_type<SliceType>(char_type, false);
  auto char_ref_slice = pool.make_type<ReferenceType>(char_slice, false);

  ASSERT_NE(*u8_ref_slice, *char_ref_slice);
}

/**

##### **5. TypePool Correctness**

These tests verify that the `TypePool` correctly de-duplicates complex types, even when an alias is involved in the second creation attempt.

**/

TEST_F(AstTypeTest, TypePoolCorrectness) {
  const size_t initial_pool_size = pool.size();

  // Request an existing type. No new types should be created.
  auto existing_i32 = pool.make_type<PrimitiveType>(TypePrime::I32, false);
  ASSERT_EQ(pool.size(), initial_pool_size);
  ASSERT_EQ(existing_i32, i32_type);

  // Request a new primitive type. Pool size should increase by 1.
  auto new_i8 = pool.make_type<PrimitiveType>(TypePrime::I8, false);
  ASSERT_EQ(pool.size(), initial_pool_size + 1);

  // Request a new complex type. The pool should create and store it.
  // `[u32; 1]` is new, `(u32, [u32; 1])` is new.
  auto complex_type = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{
    u32_type,
    pool.make_type<ArrayType>(u32_type, 1, false)
  }, false);
  ASSERT_EQ(pool.size(), initial_pool_size + 3);

  // Request the exact same complex type again. The pool should return the cached one.
  auto complex_type_clone = pool.make_type<TupleType>(std::vector<std::shared_ptr<ExprType>>{
    u32_type,
    pool.make_type<ArrayType>(u32_type, 1, false)
  }, false);

  ASSERT_EQ(pool.size(), initial_pool_size + 3);
  ASSERT_EQ(complex_type, complex_type_clone);
}