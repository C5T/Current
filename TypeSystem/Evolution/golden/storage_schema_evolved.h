// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_381EECA4ACB24A12
#define CURRENT_USERSPACE_381EECA4ACB24A12

#include "current.h"

// clang-format off

namespace current_userspace_381eeca4acb24a12 {

CURRENT_STRUCT(TransactionMeta) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds);
  CURRENT_FIELD(fields, (std::map<std::string, std::string>));
};
using T9201222851776140948 = TransactionMeta;

CURRENT_STRUCT(Name) {
  CURRENT_FIELD(full, std::string);
};
using T9202335020894922996 = Name;

CURRENT_STRUCT(User, Name) {
  CURRENT_FIELD(key, std::string);
};
using T9202361573173033476 = User;

CURRENT_STRUCT(PersistedUserUpdated) {
  CURRENT_FIELD(data, User);
};
using T9205376179718671976 = PersistedUserUpdated;

CURRENT_STRUCT(PersistedUserDeleted) {
  CURRENT_FIELD(key, std::string);
};
using T9202204057218879498 = PersistedUserDeleted;

CURRENT_VARIANT(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, PersistedUserUpdated, PersistedUserDeleted);
using T9224928948940686845 = Variant_B_PersistedUserUpdated_PersistedUserDeleted_E;

CURRENT_STRUCT(Transaction_T9224928948940686845) {
  CURRENT_FIELD(meta, TransactionMeta);
  CURRENT_FIELD(mutations, std::vector<Variant_B_PersistedUserUpdated_PersistedUserDeleted_E>);
};
using T9201675571270866294 = Transaction_T9224928948940686845;

}  // namespace current_userspace_381eeca4acb24a12

CURRENT_NAMESPACE(USERSPACE_381EECA4ACB24A12) {
  CURRENT_NAMESPACE_TYPE(TransactionMeta, current_userspace_381eeca4acb24a12::TransactionMeta);
  CURRENT_NAMESPACE_TYPE(Transaction_T9224928948940686845, current_userspace_381eeca4acb24a12::Transaction_T9224928948940686845);
  CURRENT_NAMESPACE_TYPE(PersistedUserDeleted, current_userspace_381eeca4acb24a12::PersistedUserDeleted);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_381eeca4acb24a12::Name);
  CURRENT_NAMESPACE_TYPE(User, current_userspace_381eeca4acb24a12::User);
  CURRENT_NAMESPACE_TYPE(PersistedUserUpdated, current_userspace_381eeca4acb24a12::PersistedUserUpdated);
  CURRENT_NAMESPACE_TYPE(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, current_userspace_381eeca4acb24a12::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E);
};  // CURRENT_NAMESPACE(USERSPACE_381EECA4ACB24A12)

namespace current {
namespace type_evolution {

// Default evolution for struct `TransactionMeta`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::TransactionMeta, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename NAMESPACE::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.timestamp), EVOLUTOR>::template Go<INTO>(from.timestamp, into.timestamp);
      Evolve<NAMESPACE, decltype(from.fields), EVOLUTOR>::template Go<INTO>(from.fields, into.fields);
  }
};

// Default evolution for struct `Transaction_T9224928948940686845`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::Transaction_T9224928948940686845, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename NAMESPACE::Transaction_T9224928948940686845& from,
                 typename INTO::Transaction_T9224928948940686845& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9224928948940686845>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.meta), EVOLUTOR>::template Go<INTO>(from.meta, into.meta);
      Evolve<NAMESPACE, decltype(from.mutations), EVOLUTOR>::template Go<INTO>(from.mutations, into.mutations);
  }
};

// Default evolution for struct `PersistedUserDeleted`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename NAMESPACE::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};

// Default evolution for struct `Name`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename NAMESPACE::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.full), EVOLUTOR>::template Go<INTO>(from.full, into.full);
  }
};

// Default evolution for struct `User`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::User, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename NAMESPACE::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::Name, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::Name&>(from), static_cast<typename INTO::Name&>(into));
      Evolve<NAMESPACE, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};

// Default evolution for struct `PersistedUserUpdated`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename NAMESPACE::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_381EECA4ACB24A12_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases {
  DST& into;
  explicit USERSPACE_381EECA4ACB24A12_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases(DST& into) : into(into) {}
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
template <typename NAMESPACE, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<NAMESPACE, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_381EECA4ACB24A12_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));
  }
};

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_381EECA4ACB24A12
