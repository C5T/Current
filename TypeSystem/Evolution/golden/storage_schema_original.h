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

#ifndef CURRENT_SCHEMA_FOR_T9203533648527088493
#define CURRENT_SCHEMA_FOR_T9203533648527088493
namespace t9203533648527088493 {
CURRENT_STRUCT(Name) {
  CURRENT_FIELD(first, std::string);
  CURRENT_FIELD(last, std::string);
};
}  // namespace t9203533648527088493
#endif  // CURRENT_SCHEMA_FOR_T_9203533648527088493

#ifndef CURRENT_SCHEMA_FOR_T9207102759476147844
#define CURRENT_SCHEMA_FOR_T9207102759476147844
namespace t9207102759476147844 {
CURRENT_STRUCT(User, t9203533648527088493::Name) {
  CURRENT_FIELD(key, std::string);
};
}  // namespace t9207102759476147844
#endif  // CURRENT_SCHEMA_FOR_T_9207102759476147844

#ifndef CURRENT_SCHEMA_FOR_T9209900071115339734
#define CURRENT_SCHEMA_FOR_T9209900071115339734
namespace t9209900071115339734 {
CURRENT_STRUCT(PersistedUserUpdated) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(data, t9207102759476147844::User);
};
}  // namespace t9209900071115339734
#endif  // CURRENT_SCHEMA_FOR_T_9209900071115339734

#ifndef CURRENT_SCHEMA_FOR_T9200749442651087763
#define CURRENT_SCHEMA_FOR_T9200749442651087763
namespace t9200749442651087763 {
CURRENT_STRUCT(PersistedUserDeleted) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(key, std::string);
};
}  // namespace t9200749442651087763
#endif  // CURRENT_SCHEMA_FOR_T_9200749442651087763

#ifndef CURRENT_SCHEMA_FOR_T9226378158835221611
#define CURRENT_SCHEMA_FOR_T9226378158835221611
namespace t9226378158835221611 {
CURRENT_VARIANT(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, t9209900071115339734::PersistedUserUpdated, t9200749442651087763::PersistedUserDeleted);
}  // namespace t9226378158835221611
#endif  // CURRENT_SCHEMA_FOR_T_9226378158835221611

#ifndef CURRENT_SCHEMA_FOR_T9202421793643333348
#define CURRENT_SCHEMA_FOR_T9202421793643333348
namespace t9202421793643333348 {
CURRENT_STRUCT(Transaction_T9226378158835221611) {
  CURRENT_FIELD(meta, t9206905014308449807::TransactionMeta);
  CURRENT_FIELD(mutations, std::vector<t9226378158835221611::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E>);
};
}  // namespace t9202421793643333348
#endif  // CURRENT_SCHEMA_FOR_T_9202421793643333348

}  // namespace current_userspace

#ifndef CURRENT_NAMESPACE_SchemaOriginalStorage_DEFINED
#define CURRENT_NAMESPACE_SchemaOriginalStorage_DEFINED
CURRENT_NAMESPACE(SchemaOriginalStorage) {
  CURRENT_NAMESPACE_TYPE(PersistedUserDeleted, current_userspace::t9200749442651087763::PersistedUserDeleted);
  CURRENT_NAMESPACE_TYPE(Transaction_T9226378158835221611, current_userspace::t9202421793643333348::Transaction_T9226378158835221611);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace::t9203533648527088493::Name);
  CURRENT_NAMESPACE_TYPE(TransactionMeta, current_userspace::t9206905014308449807::TransactionMeta);
  CURRENT_NAMESPACE_TYPE(User, current_userspace::t9207102759476147844::User);
  CURRENT_NAMESPACE_TYPE(PersistedUserUpdated, current_userspace::t9209900071115339734::PersistedUserUpdated);
  CURRENT_NAMESPACE_TYPE(Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, current_userspace::t9226378158835221611::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E);

  // Privileged types.
  CURRENT_NAMESPACE_TYPE(Transaction, current_userspace::t9202421793643333348::Transaction_T9226378158835221611);
};  // CURRENT_NAMESPACE(SchemaOriginalStorage)
#endif  // CURRENT_NAMESPACE_SchemaOriginalStorage_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `PersistedUserDeleted`.
#ifndef DEFAULT_EVOLUTION_933F2982C4EB6266CEAA498BE945C231B5B436DAF5D36057052D3E52D3D149C7  // typename SchemaOriginalStorage::PersistedUserDeleted
#define DEFAULT_EVOLUTION_933F2982C4EB6266CEAA498BE945C231B5B436DAF5D36057052D3E52D3D149C7  // typename SchemaOriginalStorage::PersistedUserDeleted
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaOriginalStorage, typename SchemaOriginalStorage::PersistedUserDeleted, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaOriginalStorage;
  template <typename INTO>
  static void Go(const typename FROM::PersistedUserDeleted& from,
                 typename INTO::PersistedUserDeleted& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserDeleted>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(us);
      CURRENT_EVOLVE_FIELD(key);
  }
};
#endif

// Default evolution for struct `Transaction_T9226378158835221611`.
#ifndef DEFAULT_EVOLUTION_147AC364CD85AC24C0064E6AB9FE2491A1F9B25993BB8D342FCD5E810E4CB628  // typename SchemaOriginalStorage::Transaction_T9226378158835221611
#define DEFAULT_EVOLUTION_147AC364CD85AC24C0064E6AB9FE2491A1F9B25993BB8D342FCD5E810E4CB628  // typename SchemaOriginalStorage::Transaction_T9226378158835221611
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaOriginalStorage, typename SchemaOriginalStorage::Transaction_T9226378158835221611, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaOriginalStorage;
  template <typename INTO>
  static void Go(const typename FROM::Transaction_T9226378158835221611& from,
                 typename INTO::Transaction_T9226378158835221611& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Transaction_T9226378158835221611>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(meta);
      CURRENT_EVOLVE_FIELD(mutations);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_D4E373F8EC6784E64A697CA1599513E7FF3DAF19D68AF0B7313570D8697F4B56  // typename SchemaOriginalStorage::Name
#define DEFAULT_EVOLUTION_D4E373F8EC6784E64A697CA1599513E7FF3DAF19D68AF0B7313570D8697F4B56  // typename SchemaOriginalStorage::Name
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaOriginalStorage, typename SchemaOriginalStorage::Name, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaOriginalStorage;
  template <typename INTO>
  static void Go(const typename FROM::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(first);
      CURRENT_EVOLVE_FIELD(last);
  }
};
#endif

// Default evolution for struct `TransactionMeta`.
#ifndef DEFAULT_EVOLUTION_98E5AF3D217D1BA48E18D31EB43E76BFAD334ADE09A0DB39F83B1B4C5D58E97B  // typename SchemaOriginalStorage::TransactionMeta
#define DEFAULT_EVOLUTION_98E5AF3D217D1BA48E18D31EB43E76BFAD334ADE09A0DB39F83B1B4C5D58E97B  // typename SchemaOriginalStorage::TransactionMeta
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaOriginalStorage, typename SchemaOriginalStorage::TransactionMeta, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaOriginalStorage;
  template <typename INTO>
  static void Go(const typename FROM::TransactionMeta& from,
                 typename INTO::TransactionMeta& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TransactionMeta>::value == 3,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(begin_us);
      CURRENT_EVOLVE_FIELD(end_us);
      CURRENT_EVOLVE_FIELD(fields);
  }
};
#endif

// Default evolution for struct `User`.
#ifndef DEFAULT_EVOLUTION_8609764D548CB5C44B3D33735B2A0ADF36E66DDD4F48D82D7A517C7002D10EB9  // typename SchemaOriginalStorage::User
#define DEFAULT_EVOLUTION_8609764D548CB5C44B3D33735B2A0ADF36E66DDD4F48D82D7A517C7002D10EB9  // typename SchemaOriginalStorage::User
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaOriginalStorage, typename SchemaOriginalStorage::User, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaOriginalStorage;
  template <typename INTO>
  static void Go(const typename FROM::User& from,
                 typename INTO::User& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::User>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_SUPER(Name);
      CURRENT_EVOLVE_FIELD(key);
  }
};
#endif

// Default evolution for struct `PersistedUserUpdated`.
#ifndef DEFAULT_EVOLUTION_5A40E880BF75C77828A1F910FD406F93DB2F96F560CB920B2833F8F45867F351  // typename SchemaOriginalStorage::PersistedUserUpdated
#define DEFAULT_EVOLUTION_5A40E880BF75C77828A1F910FD406F93DB2F96F560CB920B2833F8F45867F351  // typename SchemaOriginalStorage::PersistedUserUpdated
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaOriginalStorage, typename SchemaOriginalStorage::PersistedUserUpdated, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaOriginalStorage;
  template <typename INTO>
  static void Go(const typename FROM::PersistedUserUpdated& from,
                 typename INTO::PersistedUserUpdated& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::PersistedUserUpdated>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(us);
      CURRENT_EVOLVE_FIELD(data);
  }
};
#endif

// Default evolution for `Variant<PersistedUserUpdated, PersistedUserDeleted>`.
#ifndef DEFAULT_EVOLUTION_9B9BB1DBA718FAB8B59F2D2448193244E6ECD75BE4099F766DE8FD8E252C86F4  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaOriginalStorage::PersistedUserUpdated, SchemaOriginalStorage::PersistedUserDeleted>>
#define DEFAULT_EVOLUTION_9B9BB1DBA718FAB8B59F2D2448193244E6ECD75BE4099F766DE8FD8E252C86F4  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaOriginalStorage::PersistedUserUpdated, SchemaOriginalStorage::PersistedUserDeleted>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaOriginalStorage_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases {
  DST& into;
  explicit SchemaOriginalStorage_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases(DST& into) : into(into) {}
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
struct Evolve<SchemaOriginalStorage, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaOriginalStorage::PersistedUserUpdated, SchemaOriginalStorage::PersistedUserDeleted>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaOriginalStorage::PersistedUserUpdated, SchemaOriginalStorage::PersistedUserDeleted>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaOriginalStorage_Variant_B_PersistedUserUpdated_PersistedUserDeleted_E_Cases<decltype(into), SchemaOriginalStorage, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_TYPE_EVOLVER(CustomEvolver, SchemaOriginalStorage, PersistedUserDeleted, {
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.us, into.us);
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, SchemaOriginalStorage, Transaction_T9226378158835221611, {
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.meta, into.meta);
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.mutations, into.mutations);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, SchemaOriginalStorage, Name, {
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.first, into.first);
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.last, into.last);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, SchemaOriginalStorage, TransactionMeta, {
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.begin_us, into.begin_us);
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.end_us, into.end_us);
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.fields, into.fields);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, SchemaOriginalStorage, User, {
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.key, into.key);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, SchemaOriginalStorage, PersistedUserUpdated, {
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.us, into.us);
  CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaOriginalStorage, t9226378158835221611::Variant_B_PersistedUserUpdated_PersistedUserDeleted_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9209900071115339734::PersistedUserUpdated, CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9200749442651087763::PersistedUserDeleted, CURRENT_NATURAL_EVOLVE(SchemaOriginalStorage, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolvers.

// clang-format on
