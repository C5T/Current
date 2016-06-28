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
#ifndef DEFAULT_EVOLUTION_0C84A77ED08C9B4B441113D0BDD9AB37F7F9FD786257AFBB37B6BAC76125249B  // typename USERSPACE_381EECA4ACB24A12::TransactionMeta
#define DEFAULT_EVOLUTION_0C84A77ED08C9B4B441113D0BDD9AB37F7F9FD786257AFBB37B6BAC76125249B  // typename USERSPACE_381EECA4ACB24A12::TransactionMeta
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_381EECA4ACB24A12::TransactionMeta, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename USERSPACE_381EECA4ACB24A12::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.timestamp), EVOLUTOR>::template Go<INTO>(from.timestamp, into.timestamp);
      Evolve<FROM, decltype(from.fields), EVOLUTOR>::template Go<INTO>(from.fields, into.fields);
  }
};
#endif

// Default evolution for struct `Transaction_T9224928948940686845`.
#ifndef DEFAULT_EVOLUTION_F7E0C5435B5E3745E9D4E8803D4821E7742B1553F5E418A508A51FC46429249D  // typename USERSPACE_381EECA4ACB24A12::Transaction_T9224928948940686845
#define DEFAULT_EVOLUTION_F7E0C5435B5E3745E9D4E8803D4821E7742B1553F5E418A508A51FC46429249D  // typename USERSPACE_381EECA4ACB24A12::Transaction_T9224928948940686845
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_381EECA4ACB24A12::Transaction_T9224928948940686845, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename USERSPACE_381EECA4ACB24A12::Transaction_T9224928948940686845& from,
                 typename INTO::Transaction_T9224928948940686845& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9224928948940686845>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.meta), EVOLUTOR>::template Go<INTO>(from.meta, into.meta);
      Evolve<FROM, decltype(from.mutations), EVOLUTOR>::template Go<INTO>(from.mutations, into.mutations);
  }
};
#endif

// Default evolution for struct `PersistedUserDeleted`.
#ifndef DEFAULT_EVOLUTION_99F650FFF4846948BAAB90957EFDDDEB422C421994B3A709EA706DFA20EDF57B  // typename USERSPACE_381EECA4ACB24A12::PersistedUserDeleted
#define DEFAULT_EVOLUTION_99F650FFF4846948BAAB90957EFDDDEB422C421994B3A709EA706DFA20EDF57B  // typename USERSPACE_381EECA4ACB24A12::PersistedUserDeleted
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_381EECA4ACB24A12::PersistedUserDeleted, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename USERSPACE_381EECA4ACB24A12::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_5E4A8DA7997965A9CFD8D3ED1B4E4911BB25BEA0DDF430C8639E9EE60038B6A0  // typename USERSPACE_381EECA4ACB24A12::Name
#define DEFAULT_EVOLUTION_5E4A8DA7997965A9CFD8D3ED1B4E4911BB25BEA0DDF430C8639E9EE60038B6A0  // typename USERSPACE_381EECA4ACB24A12::Name
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_381EECA4ACB24A12::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename USERSPACE_381EECA4ACB24A12::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.full), EVOLUTOR>::template Go<INTO>(from.full, into.full);
  }
};
#endif

// Default evolution for struct `User`.
#ifndef DEFAULT_EVOLUTION_61CBA7CD4239407FC0A4561F95F9C5AA3E3F416D281CA7F80EE5BD89FACD0A82  // typename USERSPACE_381EECA4ACB24A12::User
#define DEFAULT_EVOLUTION_61CBA7CD4239407FC0A4561F95F9C5AA3E3F416D281CA7F80EE5BD89FACD0A82  // typename USERSPACE_381EECA4ACB24A12::User
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_381EECA4ACB24A12::User, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename USERSPACE_381EECA4ACB24A12::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_381EECA4ACB24A12::Name, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::Name&>(from), static_cast<typename INTO::Name&>(into));
      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);
  }
};
#endif

// Default evolution for struct `PersistedUserUpdated`.
#ifndef DEFAULT_EVOLUTION_75FC8606DD5D5A0EEE2C024A859F2D0C60C0FC1D27F93482F88397E0F7A52762  // typename USERSPACE_381EECA4ACB24A12::PersistedUserUpdated
#define DEFAULT_EVOLUTION_75FC8606DD5D5A0EEE2C024A859F2D0C60C0FC1D27F93482F88397E0F7A52762  // typename USERSPACE_381EECA4ACB24A12::PersistedUserUpdated
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const typename USERSPACE_381EECA4ACB24A12::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
#ifndef DEFAULT_EVOLUTION_D98BDC380064339D9FD734CFAA7C5D5E53FD3D98B14E64F0B91A071F4630A3CA  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted>>
#define DEFAULT_EVOLUTION_D98BDC380064339D9FD734CFAA7C5D5E53FD3D98B14E64F0B91A071F4630A3CA  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted>>
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
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_381EECA4ACB24A12, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_381EECA4ACB24A12::PersistedUserUpdated, USERSPACE_381EECA4ACB24A12::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_381EECA4ACB24A12_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolutors.

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_381EECA4ACB24A12, TransactionMeta, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.timestamp, into.timestamp);
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.fields, into.fields);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_381EECA4ACB24A12, Transaction_T9224928948940686845, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.meta, into.meta);
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.mutations, into.mutations);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_381EECA4ACB24A12, PersistedUserDeleted, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_381EECA4ACB24A12, Name, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.full, into.full);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_381EECA4ACB24A12, User, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_381EECA4ACB24A12, PersistedUserUpdated, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, USERSPACE_381EECA4ACB24A12, Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(PersistedUserUpdated, CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(PersistedUserDeleted, CURRENT_NATURAL_EVOLVE(USERSPACE_381EECA4ACB24A12, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolutors.

// clang-format on

#endif  // CURRENT_USERSPACE_381EECA4ACB24A12
