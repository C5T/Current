#include <vector>
#include <string>

#include "reflection.h"
#include "../Bricks/strings/join.h"

// TODO(dkorolev): Use RapidJSON from outside Cereal.
#include "../3rdparty/cereal/include/external/rapidjson/document.h"
#include "../3rdparty/cereal/include/external/rapidjson/prettywriter.h"
#include "../3rdparty/cereal/include/external/rapidjson/genericstream.h"

#include "../3rdparty/gtest/gtest-main.h"

namespace reflection_test {

// A few properly defined Current data types.
CURRENT_STRUCT(Foo) { CURRENT_FIELD(i, uint64_t, 42u); };
CURRENT_STRUCT(Bar) {
  CURRENT_FIELD(v1, std::vector<uint64_t>);
  CURRENT_FIELD(v2, std::vector<Foo>);
  CURRENT_FIELD(v3, std::vector<std::vector<Foo>>);
};
CURRENT_STRUCT(DerivedFromFoo, Foo) { CURRENT_FIELD(bar, Bar); };

static_assert(IS_VALID_CURRENT_STRUCT(Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(Bar), "Struct `Bar` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(DerivedFromFoo), "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace reflection_test

// Confirm that Current data types defined in a namespace are accessible from outside it.
static_assert(IS_VALID_CURRENT_STRUCT(reflection_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(reflection_test::Bar), "Struct `Bar` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(reflection_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

namespace some_other_namespace {

// Confirm that Current data types defined in one namespace are accessible from another one.
static_assert(IS_VALID_CURRENT_STRUCT(::reflection_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::reflection_test::Bar), "Struct `Bar` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::reflection_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace some_other_namespace

namespace valid_struct_test {

// Properly declared structures.
CURRENT_STRUCT(Empty){};
CURRENT_STRUCT(EmptyDerived, Empty){};

static_assert(IS_VALID_CURRENT_STRUCT(Empty), "`Empty` must pass `IS_VALID_CURRENT_STRUCT` check.");
static_assert(IS_VALID_CURRENT_STRUCT(EmptyDerived),
              "`EmptyDerived` must pass `IS_VALID_CURRENT_STRUCT` check.");

// Improperly declared structures.
struct WrongStructNotCurrentStruct {
  int x;
};
struct WrongDerivedStructNotCurrentStruct : ::current::reflection::CurrentSuper {};
struct NotCurrentStructDerivedFromCurrentStruct : Empty {};

CURRENT_STRUCT(WrongUsesCOUNTERInternally) {
  CURRENT_FIELD(i1, uint64_t);
  static size_t GetCounter() { return __COUNTER__; }
  CURRENT_FIELD(i2, uint64_t);
};

// The lines below don't compile with various errors.
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongStructNotCurrentStruct),
//               "`WrongStructNotCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongDerivedStructNotCurrentStruct),
//               "`WrongDerivedStructNotCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(NotCurrentStructDerivedFromCurrentStruct),
//               "`NotCurrentStructDerivedFromCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongUsesCOUNTERInternally),
//               "`WrongUsesCOUNTERInternally` must not pass `IS_VALID_CURRENT_STRUCT` check.");

}  // namespace valid_struct_test

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

  EXPECT_EQ(
      "struct DerivedFromFoo : Foo {\n"
      "  Bar bar;\n"
      "};\n",
      Reflector().DescribeCppStruct<DerivedFromFoo>());

  EXPECT_EQ(7u, Reflector().KnownTypesCountForUnitTest());
}

TEST(Reflection, TypeID) {
  using namespace reflection_test;
  using current::reflection::ReflectedType_Struct;

  // TODO(dkorolev): Migrate to `Polymorphic<>` and avoid `dynamic_cast<>` here.
  const ReflectedType_Struct& bar = dynamic_cast<const ReflectedType_Struct&>(*Reflector().ReflectType<Bar>());
  EXPECT_EQ(9010000001031372545ull, static_cast<uint64_t>(bar.fields[0].first->type_id));
  EXPECT_EQ(9010000003023971265ull, static_cast<uint64_t>(bar.fields[1].first->type_id));
  EXPECT_EQ(9010000000769980382ull, static_cast<uint64_t>(bar.fields[2].first->type_id));
}

TEST(Reflection, CurrentStructInternals) {
  using namespace reflection_test;
  using namespace current::reflection;

  static_assert(std::is_same<SuperType<Foo>, CurrentSuper>::value, "");
  EXPECT_EQ(1u, FieldCounter<Foo>::value);

  std::string field_name;
  Foo::CURRENT_REFLECTION([&field_name](const std::string& name) { field_name = name; }, Index<FieldName, 0>());
  EXPECT_EQ("i", field_name);

  bool field_type_correct = false;
  Foo::CURRENT_REFLECTION([&field_type_correct](TypeSelector<uint64_t>) { field_type_correct = true; },
                          Index<FieldType, 0>());
  EXPECT_TRUE(field_type_correct);

  Foo foo;
  uint64_t field_value = 0u;
  foo.CURRENT_REFLECTION([&field_value](uint64_t value) { field_value = value; }, Index<FieldValue, 0>());
  EXPECT_EQ(42u, field_value);

  foo.i = 100u;
  foo.CURRENT_REFLECTION([&field_name, &field_value](const std::string& name, const uint64_t& value) {
    field_name = name;
    field_value = value;
  }, Index<FieldNameAndImmutableValue, 0>());
  EXPECT_EQ("i", field_name);
  EXPECT_EQ(100u, field_value);

  foo.CURRENT_REFLECTION([&field_name](const std::string& name, uint64_t& value) {
    field_name = name;
    value = 123u;
  }, Index<FieldNameAndMutableValue, 0>());
  EXPECT_EQ("i", field_name);
  EXPECT_EQ(123u, foo.i);

  static_assert(std::is_same<SuperType<Bar>, CurrentSuper>::value, "");
  EXPECT_EQ(3u, FieldCounter<Bar>::value);
  static_assert(std::is_same<SuperType<DerivedFromFoo>, Foo>::value, "");
  EXPECT_EQ(1u, FieldCounter<DerivedFromFoo>::value);
}

namespace reflection_test {

CURRENT_STRUCT(Serializable) {
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(s, std::string);
};

CURRENT_STRUCT(ComplexSerializable) {
  CURRENT_FIELD(j, uint64_t);
  CURRENT_FIELD(q, std::string);
  CURRENT_FIELD(v, std::vector<std::string>);
  CURRENT_FIELD(z, Serializable);
};

// TODO(dkorolev): When doing serialization, iterate over the fields of the base class too.

struct CollectFieldNames {
  std::vector<std::string>& output_;
  template <typename T>
  void operator()(current::reflection::TypeSelector<T>, const std::string& name) const {
    output_.push_back(name);
  }
};

}  // namespace reflection_test

TEST(Reflection, VisitAllFields) {
  using namespace reflection_test;

  EXPECT_EQ(
      "struct Serializable {\n"
      "  uint64_t i;\n"
      "  std::string s;\n"
      "};\n",
      Reflector().DescribeCppStruct<Serializable>());
  {
    std::vector<std::string> result;
    CollectFieldNames names{result};
    current::reflection::VisitAllFields<Serializable, current::reflection::FieldTypeAndName>::WithoutObject(
        names);
    EXPECT_EQ("i,s", bricks::strings::Join(result, ','));
  }
  {
    std::vector<std::string> result;
    CollectFieldNames names{result};
    current::reflection::VisitAllFields<ComplexSerializable,
                                        current::reflection::FieldTypeAndName>::WithoutObject(names);
    EXPECT_EQ("j,q,v,z", bricks::strings::Join(result, ','));
  }
}

namespace reflection_json {

template <typename T>
struct SaveIntoJSONImpl;

template <typename T>
struct AssignToRapidJSONValueImpl {
  static void WithDedicatedStringTreatment(rapidjson::Value& destination, const T& value) {
    destination = value;
  }
};

template <>
struct AssignToRapidJSONValueImpl<std::string> {
  static void WithDedicatedStringTreatment(rapidjson::Value& destination, const std::string& value) {
    destination.SetString(value.c_str(), value.length());
  }
};

template <typename T>
void AssignToRapidJSONValue(rapidjson::Value& destination, const T& value) {
  AssignToRapidJSONValueImpl<T>::WithDedicatedStringTreatment(destination, value);
}

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, unused_current_type) \
  template <>                                                                              \
  struct SaveIntoJSONImpl<cpp_type> {                                                      \
    static void Go(rapidjson::Value& destination,                                          \
                   rapidjson::Document::AllocatorType&,                                    \
                   const cpp_type& value) {                                                \
      AssignToRapidJSONValue(destination, value);                                          \
    }                                                                                      \
  };
#include "primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

// TODO(dkorolev): A smart `enable_if` to not treat any non-primitive type as a `CURRENT_STRUCT`?
template <typename T>
struct SaveIntoJSONImpl {
  struct Visitor {
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;

    Visitor(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    // IMPORTANT: Pass in `const char* name`, as `const std::string& name`
    // would fail memory-allocation-wise due to over-smartness of RapidJSON.
    template <typename U>
    void operator()(const char* name, const U& value) const {
      rapidjson::Value placeholder;
      SaveIntoJSONImpl<U>::Go(placeholder, allocator_, value);
      destination_.AddMember(name, placeholder, allocator_);
    }
  };

  static void Go(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator, const T& value) {
    destination.SetObject();

    Visitor serializer(destination, allocator);
    current::reflection::VisitAllFields<bricks::decay<T>, current::reflection::FieldNameAndImmutableValue>::
        WithObject(value, serializer);
  }
};

template <typename T>
std::string JSON(const T& value) {
  rapidjson::Document document;
  rapidjson::Value& destination = document;

  SaveIntoJSONImpl<T>::Go(destination, document.GetAllocator(), value);

  std::ostringstream os;
  auto stream = rapidjson::GenericWriteStream(os);
  auto writer = rapidjson::Writer<rapidjson::GenericWriteStream>(stream);
  document.Accept(writer);

  return os.str();
}

}  // namespace reflection_json

TEST(Reflection, SerializeIntoJSON) {
  using namespace reflection_test;
  using namespace reflection_json;

  Serializable object;
  object.i = 0;
  object.s = "";

  EXPECT_EQ("{\"i\":0,\"s\":\"\"}", JSON(object));

  object.i = 42;
  object.s = "foo";
  EXPECT_EQ("{\"i\":42,\"s\":\"foo\"}", JSON(object));
}

// RapidJSON examples framed as tests. One way we may wish to remove them. -- D.K.
TEST(RapidJSON, Smoke) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::GenericWriteStream;

  std::string json;

  {
    Document document;
    auto& allocator = document.GetAllocator();
    Value foo("bar");
    document.SetObject().AddMember("foo", foo, allocator);

    EXPECT_TRUE(document.IsObject());
    EXPECT_TRUE(document.HasMember("foo"));
    EXPECT_TRUE(document["foo"].IsString());
    EXPECT_EQ("bar", document["foo"].GetString());

    std::ostringstream os;
    auto stream = GenericWriteStream(os);
    auto writer = Writer<GenericWriteStream>(stream);
    document.Accept(writer);
    json = os.str();
  }

  EXPECT_EQ("{\"foo\":\"bar\"}", json);

  {
    Document document;
    ASSERT_FALSE(document.Parse<0>(json.c_str()).HasParseError());
    EXPECT_TRUE(document.IsObject());
    EXPECT_TRUE(document.HasMember("foo"));
    EXPECT_TRUE(document["foo"].IsString());
    EXPECT_EQ(std::string("bar"), document["foo"].GetString());
    EXPECT_FALSE(document.HasMember("bar"));
    EXPECT_FALSE(document.HasMember("meh"));
  }
}

TEST(RapidJSON, NullInString) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::GenericWriteStream;

  std::string json;

  {
    Document document;
    auto& allocator = document.GetAllocator();
    Value s;
    s.SetString("terrible\0avoided", strlen("terrible") + 1 + strlen("avoided"));
    document.SetObject();
    document.AddMember("s", s, allocator);

    std::ostringstream os;
    auto stream = GenericWriteStream(os);
    auto writer = Writer<GenericWriteStream>(stream);
    document.Accept(writer);
    json = os.str();
  }

  EXPECT_EQ("{\"s\":\"terrible\\u0000avoided\"}", json);

  {
    Document document;
    ASSERT_FALSE(document.Parse<0>(json.c_str()).HasParseError());
    EXPECT_EQ(std::string("terrible"), document["s"].GetString());
    EXPECT_EQ(std::string("terrible\0avoided", strlen("terrible") + 1 + strlen("avoided")),
              std::string(document["s"].GetString(), document["s"].GetStringLength()));
  }
}
