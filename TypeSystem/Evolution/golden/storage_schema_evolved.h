// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_229EB367F0CD061F
#define CURRENT_USERSPACE_229EB367F0CD061F

#include "current.h"

// clang-format off

namespace current_userspace_229eb367f0cd061f {

CURRENT_STRUCT(TransactionMeta) {
  CURRENT_FIELD(begin_us, std::chrono::microseconds);
  CURRENT_FIELD(end_us, std::chrono::microseconds);
  CURRENT_FIELD(fields, (std::map<std::string, std::string>));
};
using T9206905014308449807 = TransactionMeta;

CURRENT_STRUCT(Name) {
  CURRENT_FIELD(full, std::string);
};
using T9202335020894922996 = Name;

CURRENT_STRUCT(User, Name) {
  CURRENT_FIELD(key, std::string);
};
using T9202361573173033476 = User;

CURRENT_STRUCT(PersistedUserUpdated) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(data, User);
};
using T9208682047004194331 = PersistedUserUpdated;

CURRENT_STRUCT(PersistedUserDeleted) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(key, std::string);
};
using T9200749442651087763 = PersistedUserDeleted;

CURRENT_VARIANT(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, PersistedUserUpdated, PersistedUserDeleted);
using T9221660456409416796 = Variant_B_PersistedUserUpdated_PersistedUserDeleted_E;

CURRENT_STRUCT(Transaction_T9221660456409416796) {
  CURRENT_FIELD(meta, TransactionMeta);
  CURRENT_FIELD(mutations, std::vector<Variant_B_PersistedUserUpdated_PersistedUserDeleted_E>);
};
using T9204310366938332731 = Transaction_T9221660456409416796;

}  // namespace current_userspace_229eb367f0cd061f

CURRENT_NAMESPACE(USERSPACE_229EB367F0CD061F) {
  CURRENT_NAMESPACE_TYPE(PersistedUserDeleted, current_userspace_229eb367f0cd061f::PersistedUserDeleted);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_229eb367f0cd061f::Name);
  CURRENT_NAMESPACE_TYPE(User, current_userspace_229eb367f0cd061f::User);
  CURRENT_NAMESPACE_TYPE(Transaction_T9221660456409416796, current_userspace_229eb367f0cd061f::Transaction_T9221660456409416796);
  CURRENT_NAMESPACE_TYPE(TransactionMeta, current_userspace_229eb367f0cd061f::TransactionMeta);
  CURRENT_NAMESPACE_TYPE(PersistedUserUpdated, current_userspace_229eb367f0cd061f::PersistedUserUpdated);
  CURRENT_NAMESPACE_TYPE(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, current_userspace_229eb367f0cd061f::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E);
};  // CURRENT_NAMESPACE(USERSPACE_229EB367F0CD061F)

namespace current {
namespace type_evolution {

// Default evolution for struct `PersistedUserDeleted`.
#ifndef DEFAULT_EVOLUTION_0A339BAB3B82888B59D517EDCEFFC4C7816DA698A3EB066DA5A2A42EDA8BF617  // typename USERSPACE_229EB367F0CD061F::PersistedUserDeleted
#define DEFAULT_EVOLUTION_0A339BAB3B82888B59D517EDCEFFC4C7816DA698A3EB066DA5A2A42EDA8BF617  // typename USERSPACE_229EB367F0CD061F::PersistedUserDeleted
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_229EB367F0CD061F::PersistedUserDeleted, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const typename USERSPACE_229EB367F0CD061F::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.us), EVOLUTOR>::template Go<INTO>(from.us, into.us);
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_DA7FA55C06CE919A147E240864F678FB08B400D2ABE14C347E2DD6B2F0AE971F  // typename USERSPACE_229EB367F0CD061F::Name
#define DEFAULT_EVOLUTION_DA7FA55C06CE919A147E240864F678FB08B400D2ABE14C347E2DD6B2F0AE971F  // typename USERSPACE_229EB367F0CD061F::Name
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_229EB367F0CD061F::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const typename USERSPACE_229EB367F0CD061F::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.full), EVOLUTOR>::template Go<INTO>(from.full, into.full);
  }
};
#endif

// Default evolution for struct `User`.
#ifndef DEFAULT_EVOLUTION_5B0917F8F5D8E8C928767231A06D344B03DBBA31BB37574AFE1FB320B7093DA0  // typename USERSPACE_229EB367F0CD061F::User
#define DEFAULT_EVOLUTION_5B0917F8F5D8E8C928767231A06D344B03DBBA31BB37574AFE1FB320B7093DA0  // typename USERSPACE_229EB367F0CD061F::User
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_229EB367F0CD061F::User, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const typename USERSPACE_229EB367F0CD061F::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_229EB367F0CD061F::Name, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::Name&>(from), static_cast<typename INTO::Name&>(into));
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `Transaction_T9221660456409416796`.
#ifndef DEFAULT_EVOLUTION_43C99C2A103CD927C8CFEA637B43B44C3B61F3A16F729C306CADAE2E2FB4B527  // typename USERSPACE_229EB367F0CD061F::Transaction_T9221660456409416796
#define DEFAULT_EVOLUTION_43C99C2A103CD927C8CFEA637B43B44C3B61F3A16F729C306CADAE2E2FB4B527  // typename USERSPACE_229EB367F0CD061F::Transaction_T9221660456409416796
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_229EB367F0CD061F::Transaction_T9221660456409416796, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const typename USERSPACE_229EB367F0CD061F::Transaction_T9221660456409416796& from,
                 typename INTO::Transaction_T9221660456409416796& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9221660456409416796>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.meta), EVOLUTOR>::template Go<INTO>(from.meta, into.meta);
      Evolve<FROM, decltype(from.mutations), EVOLUTOR>::template Go<INTO>(from.mutations, into.mutations);
  }
};
#endif

// Default evolution for struct `TransactionMeta`.
#ifndef DEFAULT_EVOLUTION_0CF72BEED34E3B893454B82568F1400BF5F8C6F59CA34FBD13B5DA8CCC88F824  // typename USERSPACE_229EB367F0CD061F::TransactionMeta
#define DEFAULT_EVOLUTION_0CF72BEED34E3B893454B82568F1400BF5F8C6F59CA34FBD13B5DA8CCC88F824  // typename USERSPACE_229EB367F0CD061F::TransactionMeta
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_229EB367F0CD061F::TransactionMeta, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const typename USERSPACE_229EB367F0CD061F::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.begin_us), EVOLUTOR>::template Go<INTO>(from.begin_us, into.begin_us);
      Evolve<FROM, decltype(from.end_us), EVOLUTOR>::template Go<INTO>(from.end_us, into.end_us);
      Evolve<FROM, decltype(from.fields), EVOLUTOR>::template Go<INTO>(from.fields, into.fields);
  }
};
#endif

// Default evolution for struct `PersistedUserUpdated`.
#ifndef DEFAULT_EVOLUTION_2B6DC5319FD6619740506942818CF6677FCEA58561BA7EF53E8FFAB7F7DA5B3C  // typename USERSPACE_229EB367F0CD061F::PersistedUserUpdated
#define DEFAULT_EVOLUTION_2B6DC5319FD6619740506942818CF6677FCEA58561BA7EF53E8FFAB7F7DA5B3C  // typename USERSPACE_229EB367F0CD061F::PersistedUserUpdated
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_229EB367F0CD061F::PersistedUserUpdated, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const typename USERSPACE_229EB367F0CD061F::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.us), EVOLUTOR>::template Go<INTO>(from.us, into.us);
      Evolve<FROM, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
#ifndef DEFAULT_EVOLUTION_3EC68FEA264D032A0227934616E7F9D829E62803B7680132BBBE93B287E6EB79  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_229EB367F0CD061F::PersistedUserUpdated, USERSPACE_229EB367F0CD061F::PersistedUserDeleted>>
#define DEFAULT_EVOLUTION_3EC68FEA264D032A0227934616E7F9D829E62803B7680132BBBE93B287E6EB79  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_229EB367F0CD061F::PersistedUserUpdated, USERSPACE_229EB367F0CD061F::PersistedUserDeleted>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_229EB367F0CD061F_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases {
  DST& into;
  explicit USERSPACE_229EB367F0CD061F_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases(DST& into) : into(into) {}
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
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_229EB367F0CD061F::PersistedUserUpdated, USERSPACE_229EB367F0CD061F::PersistedUserDeleted>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_229EB367F0CD061F, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_229EB367F0CD061F::PersistedUserUpdated, USERSPACE_229EB367F0CD061F::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_229EB367F0CD061F_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolutors.

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_229EB367F0CD061F, PersistedUserDeleted, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.us, into.us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_229EB367F0CD061F, Name, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.full, into.full);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_229EB367F0CD061F, User, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_229EB367F0CD061F, Transaction_T9221660456409416796, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.meta, into.meta);
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.mutations, into.mutations);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_229EB367F0CD061F, TransactionMeta, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.begin_us, into.begin_us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.end_us, into.end_us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.fields, into.fields);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_229EB367F0CD061F, PersistedUserUpdated, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.us, into.us);
  CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, USERSPACE_229EB367F0CD061F, Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(PersistedUserUpdated, CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(PersistedUserDeleted, CURRENT_NATURAL_EVOLVE(USERSPACE_229EB367F0CD061F, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolutors.

// clang-format on

#endif  // CURRENT_USERSPACE_229EB367F0CD061F
