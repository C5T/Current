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

// * (Ordered/Unordered)Dictionary<T> <=> std::(map/unordered_map)<T_KEY, T>
//   Empty(), Size(), operator[](key), Erase(key) [, iteration, {lower/upper}_bound].
//   `T_KEY` is either the type of `T.key` or of `T.get_key()`.
//
// * (Ordered/Unordered)Matrix<T> <=> { T_ROW, T_COL } -> T, two `std::(map/unordered_map)<>`-s.
//   Entries are stored in third `std::unordered_map<std::pair<T_ROW, T_COL>, std::unique_ptr<T>>`.
//   Empty(), Size(), Rows()/Cols(), Add(cell), Delete(row, col) [, iteration, {lower/upper}_bound].
//   `T_ROW` and `T_COL` are either the type of `T.row` / `T.col`, or of `T.get_row()` / `T.get_col()`.
//
// All Current-friendly types support persistence.
//
// Only allow default constructors for containers.

#ifndef CURRENT_STORAGE_STORAGE_H
#define CURRENT_STORAGE_STORAGE_H

#include "../port.h"

#include "base.h"
#include "transaction.h"
#include "transaction_policy.h"
#include "transaction_result.h"

#include "container/dictionary.h"
#include "container/matrix.h"

#include "persister/file.h"

#include "../TypeSystem/struct.h"
#include "../TypeSystem/Serialization/json.h"
#include "../TypeSystem/optional.h"

#include "../Bricks/exception.h"
#include "../Bricks/strings/strings.h"
#include "../Bricks/time/chrono.h"

namespace current {
namespace storage {

#define CURRENT_STORAGE_FIELD_ENTRY_Dictionary_IMPL(dictionary_type, entry_type, entry_name) \
  CURRENT_STRUCT(entry_name##Updated) {                                                      \
    CURRENT_FIELD(data, entry_type);                                                         \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Updated) {}                                      \
    CURRENT_CONSTRUCTOR(entry_name##Updated)(const entry_type& value) : data(value) {}       \
  };                                                                                         \
  CURRENT_STRUCT(entry_name##Deleted) {                                                      \
    CURRENT_FIELD(key, ::current::storage::sfinae::ENTRY_KEY_TYPE<entry_type>);              \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Deleted) {}                                      \
    CURRENT_CONSTRUCTOR(entry_name##Deleted)(const entry_type& value)                        \
        : key(::current::storage::sfinae::GetKey(value)) {}                                  \
  };                                                                                         \
  struct entry_name {                                                                        \
    template <typename T, typename E1, typename E2>                                          \
    using T_FIELD_TYPE = dictionary_type<T, E1, E2>;                                         \
    using T_ENTRY = entry_type;                                                              \
    using T_KEY = ::current::storage::sfinae::ENTRY_KEY_TYPE<entry_type>;                    \
    using T_UPDATE_EVENT = entry_name##Updated;                                              \
    using T_DELETE_EVENT = entry_name##Deleted;                                              \
    using T_PERSISTED_EVENT_1 = entry_name##Updated;                                         \
    using T_PERSISTED_EVENT_2 = entry_name##Deleted;                                         \
  }

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedDictionary(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Dictionary_IMPL(UnorderedDictionary, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedDictionary(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Dictionary_IMPL(OrderedDictionary, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(matrix_type, entry_type, entry_name)    \
  CURRENT_STRUCT(entry_name##Updated) {                                                 \
    CURRENT_FIELD(data, entry_type);                                                    \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Updated) {}                                 \
    CURRENT_CONSTRUCTOR(entry_name##Updated)(const entry_type& value) : data(value) {}  \
  };                                                                                    \
  CURRENT_STRUCT(entry_name##Deleted) {                                                 \
    CURRENT_FIELD(key,                                                                  \
                  (std::pair<::current::storage::sfinae::ENTRY_ROW_TYPE<entry_type>,    \
                             ::current::storage::sfinae::ENTRY_COL_TYPE<entry_type>>)); \
    CURRENT_DEFAULT_CONSTRUCTOR(entry_name##Deleted) {}                                 \
    CURRENT_CONSTRUCTOR(entry_name##Deleted)(const entry_type& value)                   \
        : key(std::make_pair(::current::storage::sfinae::GetRow(value),                 \
                             ::current::storage::sfinae::GetCol(value))) {}             \
  };                                                                                    \
  struct entry_name {                                                                   \
    template <typename T, typename E1, typename E2>                                     \
    using T_FIELD_TYPE = matrix_type<T, E1, E2>;                                        \
    using T_ENTRY = entry_type;                                                         \
    using T_ROW = ::current::storage::sfinae::ENTRY_ROW_TYPE<entry_type>;               \
    using T_COL = ::current::storage::sfinae::ENTRY_COL_TYPE<entry_type>;               \
    using T_KEY = std::pair<T_ROW, T_COL>;                                              \
    using T_UPDATE_EVENT = entry_name##Updated;                                         \
    using T_DELETE_EVENT = entry_name##Deleted;                                         \
    using T_PERSISTED_EVENT_1 = entry_name##Updated;                                    \
    using T_PERSISTED_EVENT_2 = entry_name##Deleted;                                    \
  }

#define CURRENT_STORAGE_FIELD_ENTRY_UnorderedMatrix(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(UnorderedMatrix, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY_OrderedMatrix(entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_Matrix_IMPL(OrderedMatrix, entry_type, entry_name)

#define CURRENT_STORAGE_FIELD_ENTRY(container, entry_type, entry_name) \
  CURRENT_STORAGE_FIELD_ENTRY_##container(entry_type, entry_name)

// clang-format on

#define CURRENT_STORAGE_FIELDS_HELPERS(name)                                                                   \
  template <typename T>                                                                                        \
  struct CURRENT_STORAGE_FIELDS_HELPER;                                                                        \
  template <>                                                                                                  \
  struct CURRENT_STORAGE_FIELDS_HELPER<CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>> {     \
    constexpr static size_t CURRENT_STORAGE_FIELD_INDEX_BASE = __COUNTER__ + 1;                                \
    typedef CURRENT_STORAGE_FIELDS_##name<::current::storage::CountFields> CURRENT_STORAGE_FIELD_COUNT_STRUCT; \
  }

// TODO(dkorolev) + TODO(mzhurovich): Perhaps deprecate `FieldsCount()` in favor of a public constexpr?

// clang-format off
#define CURRENT_STORAGE_IMPLEMENTATION(name)                                                                 \
  template <typename INSTANTIATION_TYPE>                                                                     \
  struct CURRENT_STORAGE_FIELDS_##name;                                                                      \
  template <template <typename...> class PERSISTER,                                                          \
            typename FIELDS,                                                                                 \
            template <typename> class TRANSACTION_POLICY, \
            typename CUSTOM_PERSISTER_PARAM>                                                    \
  struct CURRENT_STORAGE_IMPL_##name {                                                                       \
   public:                                                                                                   \
    constexpr static size_t FIELDS_COUNT = ::current::storage::FieldCounter<FIELDS>::value;                  \
    using T_FIELDS_TYPE_LIST = ::current::storage::FieldsTypeList<FIELDS, FIELDS_COUNT>;                     \
    using T_FIELDS_VARIANT = Variant<T_FIELDS_TYPE_LIST>;                                                    \
    using T_PERSISTER = PERSISTER<T_FIELDS_TYPE_LIST, CUSTOM_PERSISTER_PARAM>;                                                       \
                                                                                                             \
   private:                                                                                                  \
    FIELDS fields_;                                                                                          \
    T_PERSISTER persister_;                                                                                  \
    TRANSACTION_POLICY<T_PERSISTER> transaction_policy_;                                   \
                                                                                                             \
   public:                                                                                                   \
    using T_FIELDS_BY_REFERENCE = FIELDS&;                                                                   \
    using T_FIELDS_BY_CONST_REFERENCE = const FIELDS&;                                                       \
    using T_TRANSACTION = ::current::storage::Transaction<T_FIELDS_VARIANT>;                                 \
    using T_TRANSACTION_META_FIELDS = ::current::storage::TransactionMetaFields;                             \
    CURRENT_STORAGE_IMPL_##name& operator=(const CURRENT_STORAGE_IMPL_##name&) = delete;                     \
    template <typename... ARGS>                                                                              \
    CURRENT_STORAGE_IMPL_##name(ARGS&&... args)                                                              \
        : persister_(std::forward<ARGS>(args)...),                                                           \
          transaction_policy_(persister_,                                                                    \
                              fields_.current_storage_mutation_journal_) {                                   \
      persister_.Replay([this](const T_FIELDS_VARIANT& entry) { entry.Call(fields_); });                     \
    }                                                                                                        \
    template <typename... ARGS>                                                                              \
    typename std::result_of<FIELDS(ARGS...)>::type                                                           \
    operator()(ARGS&&... args) { return fields_(std::forward<ARGS>(args)...); }                              \
                                                                                                             \
   public:                                                                                                   \
    template <typename F>                                                                                    \
    using T_F_RESULT = typename std::result_of<F(T_FIELDS_BY_REFERENCE)>::type;                              \
    template <typename F>                                                                                    \
    ::current::Future<::current::storage::TransactionResult<T_F_RESULT<F>>, ::current::StrictFuture::Strict> \
    Transaction(F&& f) {                                                                                     \
      return transaction_policy_.Transaction([&f, this]() { return f(fields_); });                           \
    }                                                                                                        \
    template <typename F1, typename F2>                                                                      \
    ::current::Future<::current::storage::TransactionResult<void>, ::current::StrictFuture::Strict>          \
    Transaction(F1&& f1,                                                                                     \
                F2&& f2) {                                                                                   \
      return transaction_policy_.Transaction([&f1, this]() { return f1(fields_); },                          \
                                             std::forward<F2>(f2));                                          \
    }                                                                                                        \
    void ReplayTransaction(T_TRANSACTION&& transaction, ::current::ss::IndexAndTimestamp idx_ts) {           \
      transaction_policy_.ReplayTransaction([this](T_FIELDS_VARIANT&& entry) { entry.Call(fields_); },       \
                                            std::forward<T_TRANSACTION>(transaction),                        \
                                            idx_ts);                                                         \
    }                                                                                                        \
    constexpr static size_t FieldsCount() { return FIELDS_COUNT; }                                           \
    void ExposeRawLogViaHTTP(int port, const std::string& route) {                                           \
      persister_.ExposeRawLogViaHTTP(port, route);                                                           \
    }                                                                                                        \
    typename std::result_of<decltype(&T_PERSISTER::InternalExposeStream)(T_PERSISTER)>::type                 \
    InternalExposeStream() {                                                                                 \
      return persister_.InternalExposeStream();                                                              \
    }                                                                                                        \
    void GracefulShutdown() { transaction_policy_.GracefulShutdown(); }                                      \
  };                                                                                                         \
  template <template <typename...> class PERSISTER,                                                          \
            template <typename> class TRANSACTION_POLICY =                                                   \
                ::current::storage::transaction_policy::Synchronous,                                         \
            typename CUSTOM_PERSISTER_PARAM = ::current::storage::persister::NoCustomPersisterParam> \
  using name = CURRENT_STORAGE_IMPL_##name<PERSISTER,                                                        \
                                           CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>, \
                                           TRANSACTION_POLICY, \
                                           CUSTOM_PERSISTER_PARAM>;                                              \
  CURRENT_STORAGE_FIELDS_HELPERS(name)
// clang-format on

#define CURRENT_STORAGE(name)            \
  CURRENT_STORAGE_IMPLEMENTATION(name);  \
  template <typename INSTANTIATION_TYPE> \
  struct CURRENT_STORAGE_FIELDS_##name   \
      : ::current::storage::FieldsBase<  \
            CURRENT_STORAGE_FIELDS_HELPER<CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>>>

// clang-format off
#define CURRENT_STORAGE_FIELD(field_name, entry_name)                                                         \
  using T_FIELD_CONTAINER_TYPE_##field_name = entry_name::T_FIELD_TYPE<entry_name::T_ENTRY,                   \
                                                                       entry_name::T_PERSISTED_EVENT_1,       \
                                                                       entry_name::T_PERSISTED_EVENT_2>;      \
  using T_ENTRY_TYPE_##field_name = entry_name;                                                               \
  using T_FIELD_TYPE_##field_name =                                                                           \
      ::current::storage::Field<INSTANTIATION_TYPE, T_FIELD_CONTAINER_TYPE_##field_name>;                     \
  constexpr static size_t FIELD_INDEX_##field_name =                                                          \
      CURRENT_EXPAND_MACRO(__COUNTER__) - CURRENT_STORAGE_FIELD_INDEX_BASE;                                   \
  ::current::storage::FieldInfo<entry_name::T_PERSISTED_EVENT_1, entry_name::T_PERSISTED_EVENT_2> operator()( \
      ::current::storage::FieldInfoByIndex<FIELD_INDEX_##field_name>) const {                                 \
    return ::current::storage::FieldInfo<entry_name::T_PERSISTED_EVENT_1, entry_name::T_PERSISTED_EVENT_2>(); \
  }                                                                                                           \
  std::string operator()(::current::storage::FieldNameByIndex<FIELD_INDEX_##field_name>) const {              \
    return #field_name;                                                                                       \
  }                                                                                                           \
  const T_FIELD_TYPE_##field_name& operator()(                                                                \
      ::current::storage::ImmutableFieldByIndex<FIELD_INDEX_##field_name>) const {                            \
    return field_name;                                                                                        \
  }                                                                                                           \
  template <typename F>                                                                                       \
  void operator()(::current::storage::ImmutableFieldByIndex<FIELD_INDEX_##field_name>, F&& f) const {         \
    f(field_name);                                                                                            \
  }                                                                                                           \
  template <typename F, typename RETVAL>                                                                      \
  RETVAL operator()(::current::storage::ImmutableFieldByIndexAndReturn<FIELD_INDEX_##field_name, RETVAL>,     \
                    F&& f) const {                                                                            \
    return f(field_name);                                                                                     \
  }                                                                                                           \
  template <typename F>                                                                                       \
  void operator()(::current::storage::MutableFieldByIndex<FIELD_INDEX_##field_name>, F&& f) {                 \
    f(field_name);                                                                                            \
  }                                                                                                           \
  T_FIELD_TYPE_##field_name& operator()(::current::storage::MutableFieldByIndex<FIELD_INDEX_##field_name>) {  \
    return field_name;                                                                                        \
  }                                                                                                           \
  template <typename F, typename RETVAL>                                                                      \
  RETVAL operator()(::current::storage::MutableFieldByIndexAndReturn<FIELD_INDEX_##field_name, RETVAL>,       \
                    F&& f) {                                                                                  \
    return f(field_name);                                                                                     \
  }                                                                                                           \
  template <typename F>                                                                                       \
  void operator()(::current::storage::FieldNameAndTypeByIndex<FIELD_INDEX_##field_name>, F&& f) const {       \
    f(#field_name,                                                                                            \
      ::current::storage::StorageFieldTypeSelector<T_FIELD_CONTAINER_TYPE_##field_name>(),                    \
      ::current::storage::FieldUnderlyingTypesWrapper<entry_name>());                                         \
  }                                                                                                           \
  ::current::storage::StorageExtractedFieldType<T_FIELD_CONTAINER_TYPE_##field_name> operator()(              \
      ::current::storage::FieldTypeExtractor<FIELD_INDEX_##field_name>) const {                               \
    return ::current::storage::StorageExtractedFieldType<T_FIELD_CONTAINER_TYPE_##field_name>();              \
  }                                                                                                           \
  ::current::storage::StorageExtractedFieldType<T_ENTRY_TYPE_##field_name> operator()(                        \
      ::current::storage::FieldEntryTypeExtractor<FIELD_INDEX_##field_name>) const {                          \
    return ::current::storage::StorageExtractedFieldType<T_ENTRY_TYPE_##field_name>();                        \
  }                                                                                                           \
  template <typename F, typename RETVAL>                                                                      \
  RETVAL operator()(::current::storage::FieldNameAndTypeByIndexAndReturn<FIELD_INDEX_##field_name, RETVAL>,   \
                    F&& f) const {                                                                            \
    return f(#field_name,                                                                                     \
             ::current::storage::StorageFieldTypeSelector<T_FIELD_CONTAINER_TYPE_##field_name>(),             \
             ::current::storage::FieldUnderlyingTypesWrapper<entry_name>());                                  \
  }                                                                                                           \
  T_FIELD_TYPE_##field_name field_name = T_FIELD_TYPE_##field_name(current_storage_mutation_journal_);        \
  void operator()(const entry_name::T_PERSISTED_EVENT_1& e) { field_name(e); }                                \
  void operator()(const entry_name::T_PERSISTED_EVENT_2& e) { field_name(e); }
// clang-format on

template <typename STORAGE>
using MutableFields = typename STORAGE::T_FIELDS_BY_REFERENCE;

template <typename STORAGE>
using ImmutableFields = typename STORAGE::T_FIELDS_BY_CONST_REFERENCE;

template <typename>
struct PerStorageFieldTypeImpl;

template <typename T>
using PerStorageFieldType = PerStorageFieldTypeImpl<typename T::T_REST_BEHAVIOR>;

template <>
struct PerStorageFieldTypeImpl<rest::behavior::Dictionary> {
  template <typename T_RECORD>
  static auto ExtractOrComposeKey(const T_RECORD& entry)
      -> decltype(current::storage::sfinae::GetKey(std::declval<T_RECORD>())) {
    return current::storage::sfinae::GetKey(entry);
  }
  template <typename T_DICTIONARY>
  static const T_DICTIONARY& Iterate(const T_DICTIONARY& dictionary) {
    return dictionary;
  }
};

template <>
struct PerStorageFieldTypeImpl<rest::behavior::Matrix> {
  template <typename T_RECORD>
  static auto ExtractOrComposeKey(const T_RECORD& entry)
      -> std::pair<decltype(current::storage::sfinae::GetRow(std::declval<T_RECORD>())),
                   decltype(current::storage::sfinae::GetCol(std::declval<T_RECORD>()))> {
    return std::make_pair(current::storage::sfinae::GetRow(entry), current::storage::sfinae::GetCol(entry));
  }
  template <typename T_MATRIX>
  struct Iterable {
    const T_MATRIX& matrix;
    explicit Iterable(const T_MATRIX& matrix) : matrix(matrix) {}
    using Iterator = typename T_MATRIX::WholeMatrixIterator;
    Iterator begin() const { return matrix.WholeMatrixBegin(); }
    Iterator end() const { return matrix.WholeMatrixEnd(); }
  };

  template <typename T_MATRIX>
  static Iterable<T_MATRIX> Iterate(const T_MATRIX& matrix) {
    return Iterable<T_MATRIX>(matrix);
  }
};

}  // namespace current::storage
}  // namespace current

using current::storage::MutableFields;
using current::storage::ImmutableFields;

#endif  // CURRENT_STORAGE_STORAGE_H
