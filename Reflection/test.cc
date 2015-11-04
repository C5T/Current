#include "reflection.h"

#include "../3rdparty/gtest/gtest-main.h"

namespace reflection_test {
CURRENT_STRUCT(Foo) { CURRENT_FIELD(uint64_t, i); };
CURRENT_STRUCT(Bar) {
  CURRENT_FIELD(std::vector<uint64_t>, v1);
  CURRENT_FIELD(std::vector<Foo>, v2);
  CURRENT_FIELD(std::vector<std::vector<Foo>>, v3);
};

static_assert(CURRENT_STRUCT_IS_VALID(Foo), "Struct `Foo` was not properly declared.");
static_assert(CURRENT_STRUCT_IS_VALID(Bar), "Struct `Bar` was not properly declared.");
}  // namespace reflection_test

namespace some_other_namespace {
static_assert(CURRENT_STRUCT_IS_VALID(::reflection_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(CURRENT_STRUCT_IS_VALID(::reflection_test::Bar), "Struct `Bar` was not properly declared.");
}  // namespace some_other_namespace

static_assert(CURRENT_STRUCT_IS_VALID(reflection_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(CURRENT_STRUCT_IS_VALID(reflection_test::Bar), "Struct `Bar` was not properly declared.");

using current::reflection::Reflector;

TEST(Reflection, DescribeCppStruct) {
  using namespace reflection_test;

  EXPECT_EQ(
      "struct Foo {\n"
      "  uint64_t i;\n"
      "};\n",
      Reflector().DescribeCppStruct<Foo>());

  EXPECT_EQ(
      "struct Bar {\n"
      "  std::vector<uint64_t> v1;\n"
      "  std::vector<Foo> v2;\n"
      "  std::vector<std::vector<Foo>> v3;\n"
      "};\n",
      Reflector().DescribeCppStruct<Bar>());

  EXPECT_EQ(6u, Reflector().KnownTypesCount());
}

TEST(Reflection, TypeID) {
  using namespace reflection_test;
  using current::reflection::ReflectedType_Struct;
  using current::reflection::TYPEID_COLLECTION_TYPE;
  using current::reflection::TYPEID_TYPE_RANGE;

  ReflectedType_Struct* bar = dynamic_cast<ReflectedType_Struct*>(Reflector().ReflectType<Bar>());
  EXPECT_LT(TYPEID_COLLECTION_TYPE, static_cast<uint64_t>(bar->fields[0].first->type_id));
  EXPECT_GT(TYPEID_COLLECTION_TYPE + TYPEID_TYPE_RANGE, static_cast<uint64_t>(bar->fields[0].first->type_id));
  EXPECT_LT(TYPEID_COLLECTION_TYPE, static_cast<uint64_t>(bar->fields[1].first->type_id));
  EXPECT_GT(TYPEID_COLLECTION_TYPE + TYPEID_TYPE_RANGE, static_cast<uint64_t>(bar->fields[1].first->type_id));
  EXPECT_LT(TYPEID_COLLECTION_TYPE, static_cast<uint64_t>(bar->fields[2].first->type_id));
  EXPECT_GT(TYPEID_COLLECTION_TYPE + TYPEID_TYPE_RANGE, static_cast<uint64_t>(bar->fields[2].first->type_id));
  EXPECT_NE(bar->fields[0].first->type_id, bar->fields[1].first->type_id);
  EXPECT_NE(bar->fields[1].first->type_id, bar->fields[2].first->type_id);
}
