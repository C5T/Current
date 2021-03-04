/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STORAGE_BASE_H
#define CURRENT_STORAGE_BASE_H

#include "../port.h"

#include <functional>

#include "semantics.h"
#include "transaction.h"

#include "../typesystem/struct.h"

#include "../bricks/time/chrono.h"

#include "../bricks/template/typelist.h"
#include "../bricks/template/variadic_indexes.h"

namespace current {
namespace storage {

// Instantiation types.
struct DeclareFields {};
struct CountFields {};

// Dummy type for `CountFields` instantiation type.
struct CountFieldsImplementationType {
  template <typename... T>
  CountFieldsImplementationType(T&&...) {}
  long long x[100];  // TODO(dkorolev): Fix JSON parse/serialize on Windows. Gotta go deeper. -- D.K.
};

// Field by-index accessors, helper types to enable storage fields "reflection" .
template <int N>
struct FieldInfoByIndex {};

template <int N>
struct FieldNameByIndex {};

template <int N>
struct ImmutableFieldByIndex {};

template <int N, typename RETVAL>
struct ImmutableFieldByIndexAndReturn {};

template <int N>
struct MutableFieldByIndex {};

template <int N, typename RETVAL>
struct MutableFieldByIndexAndReturn {};

template <int N>
struct FieldNameAndTypeByIndex {};

template <int N, typename RETVAL>
struct FieldNameAndTypeByIndexAndReturn {};

template <typename T>
struct StorageFieldTypeSelector;

template <int N>
struct FieldTypeExtractor {};

template <int N>
struct FieldEntryTypeExtractor {};

template <typename T>
struct StorageExtractedFieldType {
  using particular_field_t = T;
};

template <typename T>
struct ExcludeTypeFromPersistence {
  static constexpr bool excluded = false;
};

template <typename T>
struct FieldUnderlyingTypesWrapper {
  // The entry type itself.
  // For template-specialize user-defined API code with the specific entry underlying type.
  using entry_t = typename T::entry_t;

  // For the RESTful API to understand URLs.
  // Map key type for dictionaries, `size_t` for vectors, etc.
  using key_t = typename T::key_t;

  // For reflection.
  using update_event_t = typename T::update_event_t;
  using delete_event_t = typename T::delete_event_t;
};

// Fields declaration and counting.
template <typename INSTANTIATION_TYPE, typename T>
struct FieldImpl;

template <typename T>
struct FieldImpl<DeclareFields, T> {
  typedef T type;
};

template <typename T>
struct FieldImpl<CountFields, T> {
  typedef CountFieldsImplementationType type;
};

template <typename INSTANTIATION_TYPE, typename T>
using Field = typename FieldImpl<INSTANTIATION_TYPE, T>::type;

template <typename T>
struct FieldCounter {
  enum { value = sizeof(typename T::CURRENT_STORAGE_FIELD_COUNT_STRUCT) / sizeof(CountFieldsImplementationType) };
};

// Helper class to get the corresponding persisted types for each of the storage fields.
#ifdef CURRENT_STORAGE_PATCH_SUPPORT
template <typename UPDATE_EVENT, typename DELETE_EVENT, typename PATCH_EVENT_OR_VOID>
#else
template <typename UPDATE_EVENT, typename DELETE_EVENT>
#endif  // CURRENT_STORAGE_PATCH_SUPPORT
struct FieldInfo {
  using update_event_t = UPDATE_EVENT;
  using delete_event_t = DELETE_EVENT;
#ifdef CURRENT_STORAGE_PATCH_SUPPORT
  using patch_event_t = PATCH_EVENT_OR_VOID;
#endif  // CURRENT_STORAGE_PATCH_SUPPORT
};

// Persisted types list generator.
template <typename FIELDS, typename INDEXES>
struct TypeListMapperImpl;

template <typename FIELDS, int... NS>
struct TypeListMapperImpl<FIELDS, current::variadic_indexes::indexes<NS...>> {
#ifdef CURRENT_STORAGE_PATCH_SUPPORT
  using result = TypeList<typename std::invoke_result_t<FIELDS, FieldInfoByIndex<NS>>::update_event_t...,
                          typename std::invoke_result_t<FIELDS, FieldInfoByIndex<NS>>::delete_event_t...,
                          typename std::invoke_result_t<FIELDS, FieldInfoByIndex<NS>>::patch_event_t...>;
#else
  using result = TypeList<typename std::invoke_result_t<FIELDS, FieldInfoByIndex<NS>>::update_event_t...,
                          typename std::invoke_result_t<FIELDS, FieldInfoByIndex<NS>>::delete_event_t...>;
#endif  // CURRENT_STORAGE_PATCH_SUPPORT
};

#ifdef CURRENT_STORAGE_PATCH_SUPPORT
template <typename FIELDS, int COUNT>
using FieldsTypeList = current::metaprogramming::TypeListRemoveVoids<
    typename TypeListMapperImpl<FIELDS, current::variadic_indexes::generate_indexes<COUNT>>::result>;
#else
template <typename FIELDS, int COUNT>
using FieldsTypeList = typename TypeListMapperImpl<FIELDS, current::variadic_indexes::generate_indexes<COUNT>>::result;
#endif  // CURRENT_STORAGE_PATCH_SUPPORT

// `MutationJournal` keeps all the changes made during one transaction, as well as the way to rollback them.
struct MutationJournal {
  TransactionMeta transaction_meta;
  std::vector<std::unique_ptr<current::CurrentStruct>> commit_log;
  std::vector<std::function<void()>> rollback_log;

  template <typename T>
  void LogMutation(T&& entry, std::function<void()> rollback) {
    commit_log.push_back(std::make_unique<T>(std::move(entry)));
    rollback_log.push_back(rollback);
  }

  void BeforeTransaction() { transaction_meta.begin_us = current::time::Now(); }

  void AfterTransaction() { transaction_meta.end_us = current::time::Now(); }

  void Rollback() {
    for (auto rit = rollback_log.rbegin(); rit != rollback_log.rend(); ++rit) {
      (*rit)();
    }
    Clear();
  }

  void Clear() {
    transaction_meta.begin_us = std::chrono::microseconds(0);
    transaction_meta.end_us = std::chrono::microseconds(0);
    transaction_meta.fields.clear();
    commit_log.clear();
    rollback_log.clear();
  }

  void AssertEmpty() const {
    CURRENT_ASSERT(transaction_meta.begin_us.count() == 0);
    CURRENT_ASSERT(transaction_meta.end_us.count() == 0);
    CURRENT_ASSERT(transaction_meta.fields.empty());
    CURRENT_ASSERT(commit_log.empty());
    CURRENT_ASSERT(rollback_log.empty());
  }
};

template <typename BASE>
struct FieldsBase : BASE {
  FieldsBase() = default;
  FieldsBase(const FieldsBase&) = delete;
  FieldsBase& operator=(const FieldsBase&) = delete;
  FieldsBase(FieldsBase&&) = delete;
  FieldsBase& operator=(FieldsBase&&) = delete;

  MutationJournal current_storage_mutation_journal_;

  void SetTransactionMetaField(const std::string& key, const std::string& value) {
    current_storage_mutation_journal_.transaction_meta.fields[key] = value;
  }

  void EraseTransactionMetaField(const std::string& key) {
    current_storage_mutation_journal_.transaction_meta.fields.erase(key);
  }
};

// Default custom persister parameter, to enable binding Storage to a custom Stream.
namespace persister {
struct NoCustomPersisterParam {};
}  // namespace current::storage::persister

}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_BASE_H
