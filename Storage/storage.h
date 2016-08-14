/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// Current-friendly container types.

// * (Ordered/Unordered)Dictionary<T> <=> std::(map/unordered_map)<key_t, T>
//   Empty(), Size(), operator[](key), Erase(key) [, iteration, {lower/upper}_bound].
//   `key_t` is either the type of `T.key` or of `T.get_key()`.
//
// * (Ordered/Unordered)(One/Many)To(One/Many)<T> <=> { row_t, col_t } -> T, two `std::(map/unordered_map)<>`-s.
//   Entries are stored in third `std::unordered_map<std::pair<row_t, col_t>, std::unique_ptr<T>>`.
//   Empty(), Size(), Rows()/Cols(), Add(cell), Delete(row, col) [, iteration, {lower/upper}_bound].
//   `row_t` and `col_t` are either the type of `T.row` / `T.col`, or of `T.get_row()` / `T.get_col()`.
//
// All Current-friendly types support persistence.
//
// Only allow default constructors for containers.

#ifndef CURRENT_STORAGE_STORAGE_H
#define CURRENT_STORAGE_STORAGE_H

#include "../port.h"

#include <atomic>

#include "base.h"
#include "transaction.h"
#include "transaction_policy.h"
#include "transaction_result.h"

#include "container/dictionary.h"
#include "container/many_to_many.h"
#include "container/one_to_one.h"
#include "container/one_to_many.h"

#include "persister/file.h"

#include "../TypeSystem/struct.h"
#include "../TypeSystem/Serialization/json.h"
#include "../TypeSystem/optional.h"

#include "../Bricks/exception.h"
#include "../Bricks/strings/strings.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/sync/waitable_atomic.h"

namespace current {
namespace storage {

#define CURRENT_STORAGE_FIELD_ENTRY_Dictionary_IMPL(dictionary_type, entry_type, entry_name)        \
  CURRENT_STRUCT(entry_name##Updated) {                                                             \
    CURRENT_FIELD(us, std::chrono::microseconds);                                                   \
    CURRENT_FIELD(data, entry_type);                                                                \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Updated) {}                                             \
    CURRENT_CONSTRUCTOR(entry_name##Updated)(std::chrono::microseconds us, const entry_type& value) \
        : us(us), data(value) {}                                                                    \
  };                                                                                                \
  CURRENT_STRUCT(entry_name##Deleted) {                                                             \
    CURRENT_FIELD(us, std::chrono::microseconds);                                                   \
    CURRENT_FIELD(key, ::current::storage::sfinae::entry_key_t<entry_type>);                        \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Deleted) {}                                             \
    CURRENT_CONSTRUCTOR(entry_name##Deleted)(std::chrono::microseconds us, const entry_type& value) \
        : us(us), key(::current::storage::sfinae::GetKey(value)) {}                                 \
  };                                                                                                \
  struct entry_name {                                                                               \
    template <typename T, typename E1, typename E2>                                                 \
    using field_t = dictionary_type<T, E1, E2>;                                                     \
    using entry_t = entry_type;                                                                     \
    using key_t = ::current::storage::sfinae::entry_key_t<entry_type>;                              \
    using update_event_t = entry_name##Updated;                                                     \
    using delete_event_t = entry_name##Deleted;                                                     \
    using persisted_event_1_t = entry_name##Updated;                                                \
    using persisted_event_2_t = entry_name##Deleted;                                                \
  }

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedDictionary(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Dictionary_IMPL(UnorderedDictionary, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedDictionary(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Dictionary_IMPL(OrderedDictionary, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(matrix_type, entry_type, entry_name)                \
  CURRENT_STRUCT(entry_name##Updated) {                                                             \
    CURRENT_FIELD(us, std::chrono::microseconds);                                                   \
    CURRENT_FIELD(data, entry_type);                                                                \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Updated) {}                                             \
    CURRENT_CONSTRUCTOR(entry_name##Updated)(std::chrono::microseconds us, const entry_type& value) \
        : us(us), data(value) {}                                                                    \
  };                                                                                                \
  CURRENT_STRUCT(entry_name##Deleted) {                                                             \
    CURRENT_FIELD(us, std::chrono::microseconds);                                                   \
    CURRENT_FIELD(key,                                                                              \
                  (std::pair<::current::storage::sfinae::entry_row_t<entry_type>,                   \
                             ::current::storage::sfinae::entry_col_t<entry_type>>));                \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Deleted) {}                                             \
    CURRENT_CONSTRUCTOR(entry_name##Deleted)(std::chrono::microseconds us, const entry_type& value) \
        : us(us),                                                                                   \
          key(std::make_pair(::current::storage::sfinae::GetRow(value),                             \
                             ::current::storage::sfinae::GetCol(value))) {}                         \
  };                                                                                                \
  struct entry_name {                                                                               \
    template <typename T, typename E1, typename E2>                                                 \
    using field_t = matrix_type<T, E1, E2>;                                                         \
    using entry_t = entry_type;                                                                     \
    using row_t = ::current::storage::sfinae::entry_row_t<entry_type>;                              \
    using col_t = ::current::storage::sfinae::entry_col_t<entry_type>;                              \
    using key_t = std::pair<row_t, col_t>;                                                          \
    using update_event_t = entry_name##Updated;                                                     \
    using delete_event_t = entry_name##Deleted;                                                     \
    using persisted_event_1_t = entry_name##Updated;                                                \
    using persisted_event_2_t = entry_name##Deleted;                                                \
  }

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedManyToUnorderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedManyToUnorderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedManyToOrderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedManyToOrderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedManyToOrderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedManyToOrderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedManyToUnorderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedManyToUnorderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedOneToUnorderedOne(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedOneToUnorderedOne, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedOneToOrderedOne(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedOneToOrderedOne, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedOneToOrderedOne(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedOneToOrderedOne, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedOneToUnorderedOne(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedOneToUnorderedOne, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedOneToUnorderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedOneToUnorderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedOneToOrderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedOneToOrderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedOneToOrderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedOneToOrderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedOneToUnorderedMany(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedOneToUnorderedMany, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY(container, entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_##container(entry_type, entry_name)

#define CURRENT_STORAGE_FIELDS_HELPERS(name)                                                                   \
  template <typename T>                                                                                        \
  struct CURRENT_STORAGE_FIELDS_HELPER;                                                                        \
  template <>                                                                                                  \
  struct CURRENT_STORAGE_FIELDS_HELPER<CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>> {     \
    constexpr static size_t CURRENT_STORAGE_FIELD_INDEX_BASE = __COUNTER__ + 1;                                \
    typedef CURRENT_STORAGE_FIELDS_##name<::current::storage::CountFields> CURRENT_STORAGE_FIELD_COUNT_STRUCT; \
  }

using CURRENT_STORAGE_DEFAULT_PERSISTER_PARAM = persister::NoCustomPersisterParam;

template <typename T>
using CURRENT_STORAGE_DEFAULT_TRANSACTION_POLICY = transaction_policy::Synchronous<T>;

enum class StorageRole : bool { Master = true, Follower = false };

// Generic storage implementation.
template <template <typename...> class PERSISTER,
          typename FIELDS,
          template <typename> class TRANSACTION_POLICY,
          typename CUSTOM_PERSISTER_PARAM>
class GenericStorageImpl {
 public:
  enum { FIELDS_COUNT = ::current::storage::FieldCounter<FIELDS>::value };
  using fields_type_list_t = ::current::storage::FieldsTypeList<FIELDS, FIELDS_COUNT>;
  using fields_variant_t = Variant<fields_type_list_t>;
  using persister_t = PERSISTER<fields_variant_t, CUSTOM_PERSISTER_PARAM>;

 private:
  std::mutex mutex_;
  FIELDS fields_;
  WaitableAtomic<uint64_t> transactions_count_;
  persister_t persister_;
  TRANSACTION_POLICY<persister_t> transaction_policy_;
  std::atomic<StorageRole> role_;

 public:
  using fields_by_ref_t = FIELDS&;
  using fields_by_cref_t = const FIELDS&;
  using transaction_t = current::storage::Transaction<fields_variant_t>;
  using transaction_meta_fields_t = TransactionMetaFields;

  GenericStorageImpl(const GenericStorageImpl&) = delete;
  GenericStorageImpl(GenericStorageImpl&&) = delete;
  GenericStorageImpl& operator=(const GenericStorageImpl&) = delete;
  GenericStorageImpl& operator=(GenericStorageImpl&&) = delete;

  template <typename... ARGS>
  GenericStorageImpl(ARGS&&... args)
      : transactions_count_(0u),
        persister_(mutex_,
                   [this](const fields_variant_t& entry) {
                     entry.Call(fields_);
                     transactions_count_.MutableUse([](uint64_t& value) { ++value; });
                   },
                   std::forward<ARGS>(args)...),
        transaction_policy_(mutex_, persister_, fields_.current_storage_mutation_journal_) {
    role_ = (persister_.DataAuthority() == persister::PersisterDataAuthority::Own) ? StorageRole::Master
                                                                                   : StorageRole::Follower;
  }

  StorageRole GetRole() const { return role_; }

  // Used for applying updates by dispatching corresponding events.
  template <typename... ARGS>
  typename std::result_of<FIELDS(ARGS...)>::type operator()(ARGS&&... args) {
    return fields_(std::forward<ARGS>(args)...);
  }

  template <typename F>
  using f_result_t = typename std::result_of<F(fields_by_ref_t)>::type;

  template <typename F>
  ::current::Future<::current::storage::TransactionResult<f_result_t<F>>, ::current::StrictFuture::Strict>
  ReadWriteTransaction(F&& f) {
    if (role_ == StorageRole::Follower) {
      CURRENT_THROW(ReadWriteTransactionInFollowerStorageException());
    }
    return transaction_policy_.Transaction([&f, this]() { return f(fields_); });
  }

  template <typename F1, typename F2>
  ::current::Future<::current::storage::TransactionResult<void>, ::current::StrictFuture::Strict>
  ReadWriteTransaction(F1&& f1, F2&& f2) {
    if (role_ == StorageRole::Follower) {
      CURRENT_THROW(ReadWriteTransactionInFollowerStorageException());
    }
    return transaction_policy_.Transaction([&f1, this]() { return f1(fields_); }, std::forward<F2>(f2));
  }

  template <typename F>
  ::current::Future<::current::storage::TransactionResult<f_result_t<F>>, ::current::StrictFuture::Strict>
  ReadOnlyTransaction(F&& f) const {
    return transaction_policy_.Transaction([&f, this]() { return f(static_cast<const FIELDS&>(fields_)); });
  }

  template <typename F1, typename F2>
  ::current::Future<::current::storage::TransactionResult<void>, ::current::StrictFuture::Strict>
  ReadOnlyTransaction(F1&& f1, F2&& f2) const {
    return transaction_policy_.Transaction([&f1, this]() { return f1(static_cast<const FIELDS&>(fields_)); },
                                           std::forward<F2>(f2));
  }

  void ExposeRawLogViaHTTP(int port, const std::string& route) { persister_.ExposeRawLogViaHTTP(port, route); }

  typename std::result_of<decltype(&persister_t::InternalExposeStream)(persister_t)>::type
  InternalExposeStream() {
    return persister_.InternalExposeStream();
  }

  persister_t& InternalExposePersister() { return persister_; }

  uint64_t TransactionsCount() const { return transactions_count_.GetValue(); }

  void WaitForTransactionsCount(uint64_t count) const {
    transactions_count_.Wait([count](uint64_t value) { return value >= count; });
  }

  void FlipToMaster() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ == StorageRole::Follower) {
      persister_.template AcquireDataAuthority<current::locks::MutexLockStatus::AlreadyLocked>();
      role_ = StorageRole::Master;
    } else {
      CURRENT_THROW(StorageIsAlreadyMasterException());
    }
  }

  void GracefulShutdown() { transaction_policy_.GracefulShutdown(); }
};

#define CURRENT_STORAGE_IMPLEMENTATION(name)                                                                   \
  template <typename INSTANTIATION_TYPE>                                                                       \
  struct CURRENT_STORAGE_FIELDS_##name;                                                                        \
  template <template <typename...> class PERSISTER,                                                            \
            template <typename> class TRANSACTION_POLICY =                                                     \
                ::current::storage::CURRENT_STORAGE_DEFAULT_TRANSACTION_POLICY,                                \
            typename CUSTOM_PERSISTER_PARAM = ::current::storage::CURRENT_STORAGE_DEFAULT_PERSISTER_PARAM>     \
  using name =                                                                                                 \
      ::current::storage::GenericStorageImpl<PERSISTER,                                                        \
                                             CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>, \
                                             TRANSACTION_POLICY,                                               \
                                             CUSTOM_PERSISTER_PARAM>;                                          \
  CURRENT_STORAGE_FIELDS_HELPERS(name)

// A minimalistic `PERSISTER` which compiles with the above `CURRENT_STORAGE_IMPLEMENTATION` macro.
// Used for the sole purpose of extracting the underlying `transaction_t` type without extra template magic.
namespace persister {

template <typename MUTATIONS_VARIANT,
          template <typename> class UNDERLYING_PERSISTER,
          typename STREAM_RECORD_TYPE>
class NullStoragePersisterImpl {
 public:
  using variant_t = MUTATIONS_VARIANT;
  using transaction_t = Transaction<variant_t>;

  // NOTE(dkorolev): Commented out to not make the compiler match the type.
  // using fields_update_function_t = std::function<void(const variant_t&)>;
  // NullStoragePersisterImpl(std::mutex&, fields_update_function_t) {}

  template<typename T>
  NullStoragePersisterImpl(std::mutex&, T) {}

  void InternalExposeStream() {}

  void PersistJournal(MutationJournal& journal) { journal.Clear(); }

  PersisterDataAuthority DataAuthority() const { return PersisterDataAuthority::Own; }
};

template <typename>
struct NullPersister {};

template <typename MUTATIONS_VARIANT, typename STREAM_RECORD_TYPE = CURRENT_STORAGE_DEFAULT_PERSISTER_PARAM>
using NullStoragePersister = NullStoragePersisterImpl<MUTATIONS_VARIANT, NullPersister, STREAM_RECORD_TYPE>;

}  // namespace current::storage::persister

template <template <template <typename...> class, template <typename> class, typename> class STORAGE>
using transaction_t = typename STORAGE<persister::NullStoragePersister,
                                       CURRENT_STORAGE_DEFAULT_TRANSACTION_POLICY,
                                       CURRENT_STORAGE_DEFAULT_PERSISTER_PARAM>::transaction_t;

#define CURRENT_STORAGE(name)            \
  CURRENT_STORAGE_IMPLEMENTATION(name);  \
  template <typename INSTANTIATION_TYPE> \
  struct CURRENT_STORAGE_FIELDS_##name   \
      : ::current::storage::FieldsBase<  \
            CURRENT_STORAGE_FIELDS_HELPER<CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>>>

// clang-format off
#define CURRENT_STORAGE_FIELD(field_name, entry_name)                                                          \
  using field_container_##field_name##_t = entry_name::field_t<entry_name::entry_t,                            \
                                                                       entry_name::persisted_event_1_t,        \
                                                                       entry_name::persisted_event_2_t>;       \
  using entry_type_##field_name##_t = entry_name;                                                              \
  using field_type_##field_name##_t =                                                                          \
      ::current::storage::Field<INSTANTIATION_TYPE, field_container_##field_name##_t>;                         \
  constexpr static size_t FIELD_INDEX_##field_name =                                                           \
      CURRENT_EXPAND_MACRO(__COUNTER__) - CURRENT_STORAGE_FIELD_INDEX_BASE;                                    \
  ::current::storage::FieldInfo<entry_name::persisted_event_1_t, entry_name::persisted_event_2_t> operator()(  \
      ::current::storage::FieldInfoByIndex<FIELD_INDEX_##field_name>) const {                                  \
    return ::current::storage::FieldInfo<entry_name::persisted_event_1_t, entry_name::persisted_event_2_t>();  \
  }                                                                                                            \
  std::string operator()(::current::storage::FieldNameByIndex<FIELD_INDEX_##field_name>) const {               \
    return #field_name;                                                                                        \
  }                                                                                                            \
  const field_type_##field_name##_t& operator()(                                                               \
      ::current::storage::ImmutableFieldByIndex<FIELD_INDEX_##field_name>) const {                             \
    return field_name;                                                                                         \
  }                                                                                                            \
  template <typename F>                                                                                        \
  void operator()(::current::storage::ImmutableFieldByIndex<FIELD_INDEX_##field_name>, F&& f) const {          \
    f(field_name);                                                                                             \
  }                                                                                                            \
  template <typename F, typename RETVAL>                                                                       \
  RETVAL operator()(::current::storage::ImmutableFieldByIndexAndReturn<FIELD_INDEX_##field_name, RETVAL>,      \
                    F&& f) const {                                                                             \
    return f(field_name);                                                                                      \
  }                                                                                                            \
  template <typename F>                                                                                        \
  void operator()(::current::storage::MutableFieldByIndex<FIELD_INDEX_##field_name>, F&& f) {                  \
    f(field_name);                                                                                             \
  }                                                                                                            \
  field_type_##field_name##_t& operator()(::current::storage::MutableFieldByIndex<FIELD_INDEX_##field_name>) { \
    return field_name;                                                                                         \
  }                                                                                                            \
  template <typename F, typename RETVAL>                                                                       \
  RETVAL operator()(::current::storage::MutableFieldByIndexAndReturn<FIELD_INDEX_##field_name, RETVAL>,        \
                    F&& f) {                                                                                   \
    return f(field_name);                                                                                      \
  }                                                                                                            \
  template <typename F>                                                                                        \
  void operator()(::current::storage::FieldNameAndTypeByIndex<FIELD_INDEX_##field_name>, F&& f) const {        \
    f(#field_name,                                                                                             \
      ::current::storage::StorageFieldTypeSelector<field_container_##field_name##_t>(),                        \
      ::current::storage::FieldUnderlyingTypesWrapper<entry_name>());                                          \
  }                                                                                                            \
  ::current::storage::StorageExtractedFieldType<field_container_##field_name##_t> operator()(                  \
      ::current::storage::FieldTypeExtractor<FIELD_INDEX_##field_name>) const {                                \
    return ::current::storage::StorageExtractedFieldType<field_container_##field_name##_t>();                  \
  }                                                                                                            \
  ::current::storage::StorageExtractedFieldType<entry_type_##field_name##_t> operator()(                       \
      ::current::storage::FieldEntryTypeExtractor<FIELD_INDEX_##field_name>) const {                           \
    return ::current::storage::StorageExtractedFieldType<entry_type_##field_name##_t>();                       \
  }                                                                                                            \
  template <typename F, typename RETVAL>                                                                       \
  RETVAL operator()(::current::storage::FieldNameAndTypeByIndexAndReturn<FIELD_INDEX_##field_name, RETVAL>,    \
                    F&& f) const {                                                                             \
    return f(#field_name,                                                                                      \
             ::current::storage::StorageFieldTypeSelector<field_container_##field_name##_t>(),                 \
             ::current::storage::FieldUnderlyingTypesWrapper<entry_name>());                                   \
  }                                                                                                            \
  field_type_##field_name##_t field_name{current_storage_mutation_journal_};                                   \
  void operator()(const entry_name::persisted_event_1_t& e) { field_name(e); }                                 \
  void operator()(const entry_name::persisted_event_2_t& e) { field_name(e); }
// clang-format on

template <typename STORAGE>
using MutableFields = typename STORAGE::fields_by_ref_t;

template <typename STORAGE>
using ImmutableFields = typename STORAGE::fields_by_cref_t;

}  // namespace current::storage
}  // namespace current

using current::storage::MutableFields;
using current::storage::ImmutableFields;

#endif  // CURRENT_STORAGE_STORAGE_H
