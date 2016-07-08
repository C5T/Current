// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_D6D2A27DC90B2AB4
#define CURRENT_USERSPACE_D6D2A27DC90B2AB4

#include "current.h"

// clang-format off

namespace current_userspace_d6d2a27dc90b2ab4 {

CURRENT_STRUCT(TransactionMeta) {
  CURRENT_FIELD(begin_us, std::chrono::microseconds);
  CURRENT_FIELD(end_us, std::chrono::microseconds);
  CURRENT_FIELD(fields, (std::map<std::string, std::string>));
};
using T9206905014308449807 = TransactionMeta;

CURRENT_STRUCT(Name) {
  CURRENT_FIELD(first, std::string);
  CURRENT_FIELD(last, std::string);
};
using T9203533648527088493 = Name;

CURRENT_STRUCT(User, Name) {
  CURRENT_FIELD(key, std::string);
};
using T9207102759476147844 = User;

CURRENT_STRUCT(PersistedUserUpdated) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(data, User);
};
using T9209900071115339734 = PersistedUserUpdated;

CURRENT_STRUCT(PersistedUserDeleted) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(key, std::string);
};
using T9200749442651087763 = PersistedUserDeleted;

CURRENT_VARIANT(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, PersistedUserUpdated, PersistedUserDeleted);
using T9226378158835221611 = Variant_B_PersistedUserUpdated_PersistedUserDeleted_E;

CURRENT_STRUCT(Transaction_T9226378158835221611) {
  CURRENT_FIELD(meta, TransactionMeta);
  CURRENT_FIELD(mutations, std::vector<Variant_B_PersistedUserUpdated_PersistedUserDeleted_E>);
};
using T9202421793643333348 = Transaction_T9226378158835221611;

}  // namespace current_userspace_d6d2a27dc90b2ab4

CURRENT_NAMESPACE(USERSPACE_D6D2A27DC90B2AB4) {
  CURRENT_NAMESPACE_TYPE(PersistedUserDeleted, current_userspace_d6d2a27dc90b2ab4::PersistedUserDeleted);
  CURRENT_NAMESPACE_TYPE(Transaction_T9226378158835221611, current_userspace_d6d2a27dc90b2ab4::Transaction_T9226378158835221611);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_d6d2a27dc90b2ab4::Name);
  CURRENT_NAMESPACE_TYPE(TransactionMeta, current_userspace_d6d2a27dc90b2ab4::TransactionMeta);
  CURRENT_NAMESPACE_TYPE(User, current_userspace_d6d2a27dc90b2ab4::User);
  CURRENT_NAMESPACE_TYPE(PersistedUserUpdated, current_userspace_d6d2a27dc90b2ab4::PersistedUserUpdated);
  CURRENT_NAMESPACE_TYPE(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, current_userspace_d6d2a27dc90b2ab4::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E);
};  // CURRENT_NAMESPACE(USERSPACE_D6D2A27DC90B2AB4)

namespace current {
namespace type_evolution {

// Default evolution for struct `PersistedUserDeleted`.
#ifndef DEFAULT_EVOLUTION_2BA208CCDB97F4BD39372A994814F55258D4156457034B50517FD07AC3587C64  // typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted
#define DEFAULT_EVOLUTION_2BA208CCDB97F4BD39372A994814F55258D4156457034B50517FD07AC3587C64  // typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.us), EVOLUTOR>::template Go<INTO>(from.us, into.us);
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `Transaction_T9226378158835221611`.
#ifndef DEFAULT_EVOLUTION_D7E95A369490AA86DF2AB9C7ED39B0B5A711071D2B5D3EE40D73E79BF8D38E05  // typename USERSPACE_D6D2A27DC90B2AB4::Transaction_T9226378158835221611
#define DEFAULT_EVOLUTION_D7E95A369490AA86DF2AB9C7ED39B0B5A711071D2B5D3EE40D73E79BF8D38E05  // typename USERSPACE_D6D2A27DC90B2AB4::Transaction_T9226378158835221611
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_D6D2A27DC90B2AB4::Transaction_T9226378158835221611, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const typename USERSPACE_D6D2A27DC90B2AB4::Transaction_T9226378158835221611& from,
                 typename INTO::Transaction_T9226378158835221611& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9226378158835221611>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.meta), EVOLUTOR>::template Go<INTO>(from.meta, into.meta);
      Evolve<FROM, decltype(from.mutations), EVOLUTOR>::template Go<INTO>(from.mutations, into.mutations);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_4744A855F02AEAA0E77270A4BF0150DB0A14F4996AF1A67D37FE0F4AD47EB986  // typename USERSPACE_D6D2A27DC90B2AB4::Name
#define DEFAULT_EVOLUTION_4744A855F02AEAA0E77270A4BF0150DB0A14F4996AF1A67D37FE0F4AD47EB986  // typename USERSPACE_D6D2A27DC90B2AB4::Name
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_D6D2A27DC90B2AB4::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const typename USERSPACE_D6D2A27DC90B2AB4::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.first), EVOLUTOR>::template Go<INTO>(from.first, into.first);
      Evolve<FROM, decltype(from.last), EVOLUTOR>::template Go<INTO>(from.last, into.last);
  }
};
#endif

// Default evolution for struct `TransactionMeta`.
#ifndef DEFAULT_EVOLUTION_D6C2AA5E08CAF1AC0368544E15B2E6AC2629CF590B18BB567A777A5733DE751F  // typename USERSPACE_D6D2A27DC90B2AB4::TransactionMeta
#define DEFAULT_EVOLUTION_D6C2AA5E08CAF1AC0368544E15B2E6AC2629CF590B18BB567A777A5733DE751F  // typename USERSPACE_D6D2A27DC90B2AB4::TransactionMeta
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_D6D2A27DC90B2AB4::TransactionMeta, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const typename USERSPACE_D6D2A27DC90B2AB4::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.begin_us), EVOLUTOR>::template Go<INTO>(from.begin_us, into.begin_us);
      Evolve<FROM, decltype(from.end_us), EVOLUTOR>::template Go<INTO>(from.end_us, into.end_us);
      Evolve<FROM, decltype(from.fields), EVOLUTOR>::template Go<INTO>(from.fields, into.fields);
  }
};
#endif

// Default evolution for struct `User`.
#ifndef DEFAULT_EVOLUTION_3005109B5A95AC6646F6B6AF0DAE800CE06773398CD0CA114D4BB88366E7ECF1  // typename USERSPACE_D6D2A27DC90B2AB4::User
#define DEFAULT_EVOLUTION_3005109B5A95AC6646F6B6AF0DAE800CE06773398CD0CA114D4BB88366E7ECF1  // typename USERSPACE_D6D2A27DC90B2AB4::User
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_D6D2A27DC90B2AB4::User, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const typename USERSPACE_D6D2A27DC90B2AB4::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_D6D2A27DC90B2AB4::Name, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::Name&>(from), static_cast<typename INTO::Name&>(into));
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `PersistedUserUpdated`.
#ifndef DEFAULT_EVOLUTION_2A5419916A0B64D2601240445E5121252B92197D92508ACED3DA07732AD6A126  // typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated
#define DEFAULT_EVOLUTION_2A5419916A0B64D2601240445E5121252B92197D92508ACED3DA07732AD6A126  // typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const typename USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.us), EVOLUTOR>::template Go<INTO>(from.us, into.us);
      Evolve<FROM, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
#ifndef DEFAULT_EVOLUTION_51FECB3CD812F9F73EA30161C7CCDEE62BED054AB4543E9DD3FE304BC0660199  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated, USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted>>
#define DEFAULT_EVOLUTION_51FECB3CD812F9F73EA30161C7CCDEE62BED054AB4543E9DD3FE304BC0660199  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated, USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_D6D2A27DC90B2AB4_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases {
  DST& into;
  explicit USERSPACE_D6D2A27DC90B2AB4_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::PersistedUserUpdated& value) const {
    using into_t = typename INTO::PersistedUserUpdated;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::PersistedUserUpdated, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::PersistedUserDeleted& value) const {
    using into_t = typename INTO::PersistedUserDeleted;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::PersistedUserDeleted, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated, USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_D6D2A27DC90B2AB4, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_D6D2A27DC90B2AB4::PersistedUserUpdated, USERSPACE_D6D2A27DC90B2AB4::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_D6D2A27DC90B2AB4_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolutors.

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, PersistedUserDeleted, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.us, into.us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, Transaction_T9226378158835221611, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.meta, into.meta);
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.mutations, into.mutations);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, Name, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.first, into.first);
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.last, into.last);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, TransactionMeta, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.begin_us, into.begin_us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.end_us, into.end_us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.fields, into.fields);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, User, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, PersistedUserUpdated, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.us, into.us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, USERSPACE_D6D2A27DC90B2AB4, Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(PersistedUserUpdated, CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(PersistedUserDeleted, CURRENT_NATURAL_EVOLVE(USERSPACE_D6D2A27DC90B2AB4, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolutors.

// clang-format on

#endif  // CURRENT_USERSPACE_D6D2A27DC90B2AB4
