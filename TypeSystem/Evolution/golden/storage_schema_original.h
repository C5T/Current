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
#ifndef DEFAULT_EVOLUTION_B3761CC8B76BC9C3F3DDC72DDD61FBC136E0EA868B3870162AF127B6AF5F52E3  // typename USERSPACE_F57EAC2563CE5708::PersistedUserUpdated
#define DEFAULT_EVOLUTION_B3761CC8B76BC9C3F3DDC72DDD61FBC136E0EA868B3870162AF127B6AF5F52E3  // typename USERSPACE_F57EAC2563CE5708::PersistedUserUpdated
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename USERSPACE_F57EAC2563CE5708::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for struct `TransactionMeta`.
#ifndef DEFAULT_EVOLUTION_EEC1318ECB472FC81CD147FE94AAB844AEBA9888593B4B269255EEA965B04FA7  // typename USERSPACE_F57EAC2563CE5708::TransactionMeta
#define DEFAULT_EVOLUTION_EEC1318ECB472FC81CD147FE94AAB844AEBA9888593B4B269255EEA965B04FA7  // typename USERSPACE_F57EAC2563CE5708::TransactionMeta
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F57EAC2563CE5708::TransactionMeta, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename USERSPACE_F57EAC2563CE5708::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.timestamp), EVOLUTOR>::template Go<INTO>(from.timestamp, into.timestamp);
      Evolve<FROM, decltype(from.fields), EVOLUTOR>::template Go<INTO>(from.fields, into.fields);
  }
};
#endif

// Default evolution for struct `PersistedUserDeleted`.
#ifndef DEFAULT_EVOLUTION_E085795FF408651725DCA5B2F2C9F8FED28AFB00CFC15C7320F1747D23B7B037  // typename USERSPACE_F57EAC2563CE5708::PersistedUserDeleted
#define DEFAULT_EVOLUTION_E085795FF408651725DCA5B2F2C9F8FED28AFB00CFC15C7320F1747D23B7B037  // typename USERSPACE_F57EAC2563CE5708::PersistedUserDeleted
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F57EAC2563CE5708::PersistedUserDeleted, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename USERSPACE_F57EAC2563CE5708::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_B430A0461EADE5FA7980A4D2DEDA7A6B34502CEE25BA5695FF541BDBEF027636  // typename USERSPACE_F57EAC2563CE5708::Name
#define DEFAULT_EVOLUTION_B430A0461EADE5FA7980A4D2DEDA7A6B34502CEE25BA5695FF541BDBEF027636  // typename USERSPACE_F57EAC2563CE5708::Name
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F57EAC2563CE5708::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename USERSPACE_F57EAC2563CE5708::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.first), EVOLUTOR>::template Go<INTO>(from.first, into.first);
      Evolve<FROM, decltype(from.last), EVOLUTOR>::template Go<INTO>(from.last, into.last);
  }
};
#endif

// Default evolution for struct `User`.
#ifndef DEFAULT_EVOLUTION_1EAC4318890E57ABA5ACDC26BBD0F668BF10768E70B89130DCF73306F3C408AB  // typename USERSPACE_F57EAC2563CE5708::User
#define DEFAULT_EVOLUTION_1EAC4318890E57ABA5ACDC26BBD0F668BF10768E70B89130DCF73306F3C408AB  // typename USERSPACE_F57EAC2563CE5708::User
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F57EAC2563CE5708::User, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename USERSPACE_F57EAC2563CE5708::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_F57EAC2563CE5708::Name, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::Name&>(from), static_cast<typename INTO::Name&>(into));
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `Transaction_T9227630689129186588`.
#ifndef DEFAULT_EVOLUTION_E4FF10B2901359D6C20AAFF8B3EEFA8B772DB927748A00ECC2512098FC0EC2E0  // typename USERSPACE_F57EAC2563CE5708::Transaction_T9227630689129186588
#define DEFAULT_EVOLUTION_E4FF10B2901359D6C20AAFF8B3EEFA8B772DB927748A00ECC2512098FC0EC2E0  // typename USERSPACE_F57EAC2563CE5708::Transaction_T9227630689129186588
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F57EAC2563CE5708::Transaction_T9227630689129186588, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const typename USERSPACE_F57EAC2563CE5708::Transaction_T9227630689129186588& from,
                 typename INTO::Transaction_T9227630689129186588& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9227630689129186588>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.meta), EVOLUTOR>::template Go<INTO>(from.meta, into.meta);
      Evolve<FROM, decltype(from.mutations), EVOLUTOR>::template Go<INTO>(from.mutations, into.mutations);
  }
};
#endif

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
#ifndef DEFAULT_EVOLUTION_108A079E91AB279856D66AB277093722CC82035897CA23DC590303D960C034EC  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted>>
#define DEFAULT_EVOLUTION_108A079E91AB279856D66AB277093722CC82035897CA23DC590303D960C034EC  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted>>
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
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F57EAC2563CE5708, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F57EAC2563CE5708::PersistedUserUpdated, USERSPACE_F57EAC2563CE5708::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_F57EAC2563CE5708_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_F57EAC2563CE5708
