// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_4F76851677D2EA7C
#define CURRENT_USERSPACE_4F76851677D2EA7C

#include "current.h"

// clang-format off

namespace current_userspace_4f76851677d2ea7c {

CURRENT_STRUCT(SimpleStruct) {
  CURRENT_FIELD(x, int32_t);
  CURRENT_FIELD(y, int32_t);
  CURRENT_FIELD(z, std::string);
};
using T9200560626419622480 = SimpleStruct;

CURRENT_STRUCT(StructWithStruct) {
  CURRENT_FIELD(s, SimpleStruct);
};
using T9200847454180217914 = StructWithStruct;

CURRENT_VARIANT(Variant_B_SimpleStruct_StructWithStruct_E, SimpleStruct, StructWithStruct);
using T9221517619468071326 = Variant_B_SimpleStruct_StructWithStruct_E;

CURRENT_STRUCT(StructWithVariant) {
  CURRENT_FIELD(v, Variant_B_SimpleStruct_StructWithStruct_E);
};
using T9200946838064757591 = StructWithVariant;

CURRENT_STRUCT(Name) {
  CURRENT_FIELD(first, std::string);
  CURRENT_FIELD(last, std::string);
};
using T9203533648527088493 = Name;

}  // namespace current_userspace_4f76851677d2ea7c

CURRENT_NAMESPACE(USERSPACE_4F76851677D2EA7C) {
  CURRENT_NAMESPACE_TYPE(SimpleStruct, current_userspace_4f76851677d2ea7c::SimpleStruct);
  CURRENT_NAMESPACE_TYPE(StructWithStruct, current_userspace_4f76851677d2ea7c::StructWithStruct);
  CURRENT_NAMESPACE_TYPE(StructWithVariant, current_userspace_4f76851677d2ea7c::StructWithVariant);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_4f76851677d2ea7c::Name);
  CURRENT_NAMESPACE_TYPE(Variant_B_SimpleStruct_StructWithStruct_E, current_userspace_4f76851677d2ea7c::Variant_B_SimpleStruct_StructWithStruct_E);
};

namespace current {
namespace type_evolution {
// Default evolution for `SimpleStruct`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_4F76851677D2EA7C, USERSPACE_4F76851677D2EA7C::SimpleStruct, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_4F76851677D2EA7C::SimpleStruct& from,
                 typename INTO::SimpleStruct& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_4F76851677D2EA7C::SimpleStruct>::value == 3,
                    "Custom evolutor required.");
      Evolve<USERSPACE_4F76851677D2EA7C, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
      Evolve<USERSPACE_4F76851677D2EA7C, decltype(from.y), EVOLUTOR>::template Go<INTO>(from.y, into.y);
      Evolve<USERSPACE_4F76851677D2EA7C, decltype(from.z), EVOLUTOR>::template Go<INTO>(from.z, into.z);
  }
};

// Default evolution for `StructWithStruct`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_4F76851677D2EA7C, USERSPACE_4F76851677D2EA7C::StructWithStruct, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_4F76851677D2EA7C::StructWithStruct& from,
                 typename INTO::StructWithStruct& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_4F76851677D2EA7C::StructWithStruct>::value == 1,
                    "Custom evolutor required.");
      Evolve<USERSPACE_4F76851677D2EA7C, decltype(from.s), EVOLUTOR>::template Go<INTO>(from.s, into.s);
  }
};

// Default evolution for `StructWithVariant`.
// TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_4F76851677D2EA7C_StructWithVariant_v_Cases {
  DST& into;
  explicit USERSPACE_4F76851677D2EA7C_StructWithVariant_v_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::SimpleStruct& value) {
    using into_t = typename INTO::SimpleStruct;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::SimpleStruct, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::StructWithStruct& value) {
    using into_t = typename INTO::StructWithStruct;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::StructWithStruct, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLUTOR>
struct Evolve<USERSPACE_4F76851677D2EA7C, USERSPACE_4F76851677D2EA7C::StructWithVariant, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_4F76851677D2EA7C::StructWithVariant& from,
                 typename INTO::StructWithVariant& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_4F76851677D2EA7C::StructWithVariant>::value == 1,
                    "Custom evolutor required.");
      { USERSPACE_4F76851677D2EA7C_StructWithVariant_v_Cases<decltype(into.v), USERSPACE_4F76851677D2EA7C, INTO, EVOLUTOR> logic(into.v); from.v.Call(logic); }
  }
};

// Default evolution for `Name`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_4F76851677D2EA7C, USERSPACE_4F76851677D2EA7C::Name, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_4F76851677D2EA7C::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_4F76851677D2EA7C::Name>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_4F76851677D2EA7C, decltype(from.first), EVOLUTOR>::template Go<INTO>(from.first, into.first);
      Evolve<USERSPACE_4F76851677D2EA7C, decltype(from.last), EVOLUTOR>::template Go<INTO>(from.last, into.last);
  }
};

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_4F76851677D2EA7C
