// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_F57EAC2563CE5708
#define CURRENT_USERSPACE_F57EAC2563CE5708

#include "current.h"

// clang-format off

namespace current_userspace_f57eac2563ce5708 {

CURRENT_STRUCT(TransactionMeta) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds);
  CURRENT_FIELD(fields, (std::map<std::string, std::string>));
};
using T9201222851776140948 = TransactionMeta;

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
  CURRENT_FIELD(data, User);
};
using T9201121062548426382 = PersistedUserUpdated;

CURRENT_STRUCT(PersistedUserDeleted) {
  CURRENT_FIELD(key, std::string);
};
using T9202204057218879498 = PersistedUserDeleted;

CURRENT_VARIANT(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, PersistedUserUpdated, PersistedUserDeleted);
using T9227630689129186588 = Variant_B_PersistedUserUpdated_PersistedUserDeleted_E;

CURRENT_STRUCT(Transaction_T9227630689129186588) {
  CURRENT_FIELD(meta, TransactionMeta);
  CURRENT_FIELD(mutations, std::vector<Variant_B_PersistedUserUpdated_PersistedUserDeleted_E>);
};
using T9208078857607186789 = Transaction_T9227630689129186588;

}  // namespace current_userspace_f57eac2563ce5708

CURRENT_NAMESPACE(USERSPACE_F57EAC2563CE5708) {
  CURRENT_NAMESPACE_TYPE(PersistedUserUpdated, current_userspace_f57eac2563ce5708::PersistedUserUpdated);
  CURRENT_NAMESPACE_TYPE(TransactionMeta, current_userspace_f57eac2563ce5708::TransactionMeta);
  CURRENT_NAMESPACE_TYPE(PersistedUserDeleted, current_userspace_f57eac2563ce5708::PersistedUserDeleted);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_f57eac2563ce5708::Name);
  CURRENT_NAMESPACE_TYPE(User, current_userspace_f57eac2563ce5708::User);
  CURRENT_NAMESPACE_TYPE(Transaction_T9227630689129186588, current_userspace_f57eac2563ce5708::Transaction_T9227630689129186588);
  CURRENT_NAMESPACE_TYPE(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, current_userspace_f57eac2563ce5708::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E);
};  // CURRENT_NAMESPACE(USERSPACE_F57EAC2563CE5708)

namespace current {
namespace type_evolution {

// Default evolution for struct `PersistedUserUpdated`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename NAMESPACE::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};

// Default evolution for struct `TransactionMeta`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::TransactionMeta, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename NAMESPACE::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.timestamp), EVOLUTOR>::template Go<INTO>(from.timestamp, into.timestamp);
      Evolve<NAMESPACE, decltype(from.fields), EVOLUTOR>::template Go<INTO>(from.fields, into.fields);
  }
};

// Default evolution for struct `PersistedUserDeleted`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename NAMESPACE::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};

// Default evolution for struct `Name`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename NAMESPACE::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.first), EVOLUTOR>::template Go<INTO>(from.first, into.first);
      Evolve<NAMESPACE, decltype(from.last), EVOLUTOR>::template Go<INTO>(from.last, into.last);
  }
};

// Default evolution for struct `User`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::User, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename NAMESPACE::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::Name, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::Name&>(from), static_cast<typename INTO::Name&>(into));
      Evolve<NAMESPACE, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};

// Default evolution for struct `Transaction_T9227630689129186588`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_F57EAC2563CE5708::Transaction_T9227630689129186588, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename NAMESPACE::Transaction_T9227630689129186588& from,
                 typename INTO::Transaction_T9227630689129186588& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9227630689129186588>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.meta), EVOLUTOR>::template Go<INTO>(from.meta, into.meta);
      Evolve<NAMESPACE, decltype(from.mutations), EVOLUTOR>::template Go<INTO>(from.mutations, into.mutations);
  }
};

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_F57EAC2563CE5708_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases {
  DST& into;
  explicit USERSPACE_F57EAC2563CE5708_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases(DST& into) : into(into) {}
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
struct Evolve<NAMESPACE, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_F57EAC2563CE5708_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));
  }
};

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_F57EAC2563CE5708
