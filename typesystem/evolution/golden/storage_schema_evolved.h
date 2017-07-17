// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#include "current.h"

// clang-format off

namespace current_userspace {

#ifndef CURRENT_SCHEMA_FOR_T9206905014308449807
#define CURRENT_SCHEMA_FOR_T9206905014308449807
namespace t9206905014308449807 {
CURRENT_STRUCT(TransactionMeta) {
  CURRENT_FIELD(begin_us, std::chrono::microseconds);
  CURRENT_FIELD(end_us, std::chrono::microseconds);
  CURRENT_FIELD(fields, (std::map<std::string, std::string>));
};
}  // namespace t9206905014308449807
#endif  // CURRENT_SCHEMA_FOR_T_9206905014308449807

#ifndef CURRENT_SCHEMA_FOR_T9202335020894922996
#define CURRENT_SCHEMA_FOR_T9202335020894922996
namespace t9202335020894922996 {
CURRENT_STRUCT(Name) {
  CURRENT_FIELD(full, std::string);
};
}  // namespace t9202335020894922996
#endif  // CURRENT_SCHEMA_FOR_T_9202335020894922996

#ifndef CURRENT_SCHEMA_FOR_T9202361573173033476
#define CURRENT_SCHEMA_FOR_T9202361573173033476
namespace t9202361573173033476 {
CURRENT_STRUCT(User, t9202335020894922996::Name) {
  CURRENT_FIELD(key, std::string);
};
}  // namespace t9202361573173033476
#endif  // CURRENT_SCHEMA_FOR_T_9202361573173033476

#ifndef CURRENT_SCHEMA_FOR_T9208682047004194331
#define CURRENT_SCHEMA_FOR_T9208682047004194331
namespace t9208682047004194331 {
CURRENT_STRUCT(PersistedUserUpdated) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(data, t9202361573173033476::User);
};
}  // namespace t9208682047004194331
#endif  // CURRENT_SCHEMA_FOR_T_9208682047004194331

#ifndef CURRENT_SCHEMA_FOR_T9200749442651087763
#define CURRENT_SCHEMA_FOR_T9200749442651087763
namespace t9200749442651087763 {
CURRENT_STRUCT(PersistedUserDeleted) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(key, std::string);
};
}  // namespace t9200749442651087763
#endif  // CURRENT_SCHEMA_FOR_T_9200749442651087763

#ifndef CURRENT_SCHEMA_FOR_T9221660456409416796
#define CURRENT_SCHEMA_FOR_T9221660456409416796
namespace t9221660456409416796 {
CURRENT_VARIANT(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, t9208682047004194331::PersistedUserUpdated, t9200749442651087763::PersistedUserDeleted);
}  // namespace t9221660456409416796
#endif  // CURRENT_SCHEMA_FOR_T_9221660456409416796

#ifndef CURRENT_SCHEMA_FOR_T9204310366938332731
#define CURRENT_SCHEMA_FOR_T9204310366938332731
namespace t9204310366938332731 {
CURRENT_STRUCT(Transaction_T9221660456409416796) {
  CURRENT_FIELD(meta, t9206905014308449807::TransactionMeta);
  CURRENT_FIELD(mutations, std::vector<t9221660456409416796::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E>);
};
}  // namespace t9204310366938332731
#endif  // CURRENT_SCHEMA_FOR_T_9204310366938332731

}  // namespace current_userspace

#ifndef CURRENT_NAMESPACE_SchemaModifiedStorage_DEFINED
#define CURRENT_NAMESPACE_SchemaModifiedStorage_DEFINED
CURRENT_NAMESPACE(SchemaModifiedStorage) {
  CURRENT_NAMESPACE_TYPE(PersistedUserDeleted, current_userspace::t9200749442651087763::PersistedUserDeleted);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace::t9202335020894922996::Name);
  CURRENT_NAMESPACE_TYPE(User, current_userspace::t9202361573173033476::User);
  CURRENT_NAMESPACE_TYPE(Transaction_T9221660456409416796, current_userspace::t9204310366938332731::Transaction_T9221660456409416796);
  CURRENT_NAMESPACE_TYPE(TransactionMeta, current_userspace::t9206905014308449807::TransactionMeta);
  CURRENT_NAMESPACE_TYPE(PersistedUserUpdated, current_userspace::t9208682047004194331::PersistedUserUpdated);
  CURRENT_NAMESPACE_TYPE(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, current_userspace::t9221660456409416796::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E);

  // Privileged types.
  CURRENT_NAMESPACE_TYPE(Transaction, current_userspace::t9204310366938332731::Transaction_T9221660456409416796);
};  // CURRENT_NAMESPACE(SchemaModifiedStorage)
#endif  // CURRENT_NAMESPACE_SchemaModifiedStorage_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `PersistedUserDeleted`.
#ifndef DEFAULT_EVOLUTION_1D2A1B807AE8014F4D1B0D28A473AA863153A2359A767A509D34486A3C2A8B80  // typename SchemaModifiedStorage::PersistedUserDeleted
#define DEFAULT_EVOLUTION_1D2A1B807AE8014F4D1B0D28A473AA863153A2359A767A509D34486A3C2A8B80  // typename SchemaModifiedStorage::PersistedUserDeleted
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaModifiedStorage, typename SchemaModifiedStorage::PersistedUserDeleted, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaModifiedStorage;
  template <typename INTO>
  static void Go(const typename FROM::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 2,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(us);
      CURRENT_COPY_FIELD(key);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_1FF0D8B674D5B622EFB6ADE5DB9A02FF4BF340F65FC228AF4A748BF2E57B0457  // typename SchemaModifiedStorage::Name
#define DEFAULT_EVOLUTION_1FF0D8B674D5B622EFB6ADE5DB9A02FF4BF340F65FC228AF4A748BF2E57B0457  // typename SchemaModifiedStorage::Name
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaModifiedStorage, typename SchemaModifiedStorage::Name, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaModifiedStorage;
  template <typename INTO>
  static void Go(const typename FROM::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(full);
  }
};
#endif

// Default evolution for struct `User`.
#ifndef DEFAULT_EVOLUTION_878AB940AFD77B5554975A77F2BBADB756F0DEE3EE62CE7F7C7C85EC7BC5E07E  // typename SchemaModifiedStorage::User
#define DEFAULT_EVOLUTION_878AB940AFD77B5554975A77F2BBADB756F0DEE3EE62CE7F7C7C85EC7BC5E07E  // typename SchemaModifiedStorage::User
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaModifiedStorage, typename SchemaModifiedStorage::User, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaModifiedStorage;
  template <typename INTO>
  static void Go(const typename FROM::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_SUPER(Name);
      CURRENT_COPY_FIELD(key);
  }
};
#endif

// Default evolution for struct `Transaction_T9221660456409416796`.
#ifndef DEFAULT_EVOLUTION_C1853441F0679BE46A407C6C231C39645C74828E577DB471D4F9CA7574B9B7BD  // typename SchemaModifiedStorage::Transaction_T9221660456409416796
#define DEFAULT_EVOLUTION_C1853441F0679BE46A407C6C231C39645C74828E577DB471D4F9CA7574B9B7BD  // typename SchemaModifiedStorage::Transaction_T9221660456409416796
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaModifiedStorage, typename SchemaModifiedStorage::Transaction_T9221660456409416796, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaModifiedStorage;
  template <typename INTO>
  static void Go(const typename FROM::Transaction_T9221660456409416796& from,
                 typename INTO::Transaction_T9221660456409416796& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9221660456409416796>::value == 2,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(meta);
      CURRENT_COPY_FIELD(mutations);
  }
};
#endif

// Default evolution for struct `TransactionMeta`.
#ifndef DEFAULT_EVOLUTION_A0DC6E4D03DC4F25AFE631E3AC0734DFE967B0D2B930C3C9F7DE4656F80CBC3D  // typename SchemaModifiedStorage::TransactionMeta
#define DEFAULT_EVOLUTION_A0DC6E4D03DC4F25AFE631E3AC0734DFE967B0D2B930C3C9F7DE4656F80CBC3D  // typename SchemaModifiedStorage::TransactionMeta
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaModifiedStorage, typename SchemaModifiedStorage::TransactionMeta, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaModifiedStorage;
  template <typename INTO>
  static void Go(const typename FROM::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 3,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(begin_us);
      CURRENT_COPY_FIELD(end_us);
      CURRENT_COPY_FIELD(fields);
  }
};
#endif

// Default evolution for struct `PersistedUserUpdated`.
#ifndef DEFAULT_EVOLUTION_3B353B4EAABC9BA1F9B0E79AB218EAF6EE538F895E975D87B84EF059E8B5D3CD  // typename SchemaModifiedStorage::PersistedUserUpdated
#define DEFAULT_EVOLUTION_3B353B4EAABC9BA1F9B0E79AB218EAF6EE538F895E975D87B84EF059E8B5D3CD  // typename SchemaModifiedStorage::PersistedUserUpdated
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaModifiedStorage, typename SchemaModifiedStorage::PersistedUserUpdated, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaModifiedStorage;
  template <typename INTO>
  static void Go(const typename FROM::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 2,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(us);
      CURRENT_COPY_FIELD(data);
  }
};
#endif

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
#ifndef DEFAULT_EVOLUTION_B4142EE07135B1EFE9DF472ACD7D2253E22971D190BFCDACB36B9B2AAC839027  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaModifiedStorage::PersistedUserUpdated, SchemaModifiedStorage::PersistedUserDeleted>>
#define DEFAULT_EVOLUTION_B4142EE07135B1EFE9DF472ACD7D2253E22971D190BFCDACB36B9B2AAC839027  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaModifiedStorage::PersistedUserUpdated, SchemaModifiedStorage::PersistedUserDeleted>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaModifiedStorage_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases {
  DST& into;
  explicit SchemaModifiedStorage_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::PersistedUserUpdated& value) const {
    using into_t = typename INTO::PersistedUserUpdated;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::PersistedUserUpdated, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::PersistedUserDeleted& value) const {
    using into_t = typename INTO::PersistedUserDeleted;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::PersistedUserDeleted, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename CURRENT_ACTIVE_EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<SchemaModifiedStorage, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaModifiedStorage::PersistedUserUpdated, SchemaModifiedStorage::PersistedUserDeleted>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaModifiedStorage::PersistedUserUpdated, SchemaModifiedStorage::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaModifiedStorage_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), SchemaModifiedStorage, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaModifiedStorage, PersistedUserDeleted, {
  CURRENT_COPY_FIELD(us);
  CURRENT_COPY_FIELD(key);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaModifiedStorage, Name, {
  CURRENT_COPY_FIELD(full);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaModifiedStorage, User, {
  CURRENT_COPY_SUPER(Name);
  CURRENT_COPY_FIELD(key);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaModifiedStorage, Transaction_T9221660456409416796, {
  CURRENT_COPY_FIELD(meta);
  CURRENT_COPY_FIELD(mutations);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaModifiedStorage, TransactionMeta, {
  CURRENT_COPY_FIELD(begin_us);
  CURRENT_COPY_FIELD(end_us);
  CURRENT_COPY_FIELD(fields);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaModifiedStorage, PersistedUserUpdated, {
  CURRENT_COPY_FIELD(us);
  CURRENT_COPY_FIELD(data);
});

CURRENT_VARIANT_EVOLVER(CustomEvolver, SchemaModifiedStorage, t9221660456409416796::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, CustomDestinationNamespace) {
  CURRENT_COPY_CASE(PersistedUserUpdated);
  CURRENT_COPY_CASE(PersistedUserDeleted);
};

#endif  // Boilerplate evolvers.

// clang-format on
