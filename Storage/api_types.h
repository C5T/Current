/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_API_TYPES_H
#define CURRENT_STORAGE_API_TYPES_H

#include "storage.h"
#include "container/sfinae.h"

#include "../Blocks/HTTP/api.h"

namespace current {
namespace storage {
namespace rest {

const std::string kRESTfulDataURLComponent = "data";
const std::string kRESTfulSchemaURLComponent = "schema";

// TODO(dkorolev): The whole `FieldTypeDependentImpl` section below to be moved to `semantics.h`.
template <typename>
struct FieldTypeDependentImpl {};

template <>
struct FieldTypeDependentImpl<semantics::primary_key::Key> {
  using url_key_t = std::string;
  template <typename F_WITH, typename F_WITHOUT>
  static void CallWithOrWithoutKeyFromURL(Request&& request_rref, F_WITH&& with, F_WITHOUT&& without) {
    // TODO(dkorolev): Also accept `?row` w/o `?col`, and other types of one-of-two-keys scenarios,
    // as this `semantics::primary_key::Key` mode is used for partial access as well.
    if (request_rref.url_path_args.size() == 1u) {
      with(std::move(request_rref), request_rref.url_path_args[0]);
    } else if (request_rref.url.query.has("key")) {
      with(std::move(request_rref), request_rref.url.query["key"]);
    } else {
      without(std::move(request_rref));
    }
  }
  static std::string FormatURLKey(const std::string& key) {
    // TODO(dkorolev): Perhaps return "?key=..." in case the key contains slashes?
    return key;
  }
  template <typename KEY>
  static KEY ParseURLKey(const std::string& url_key) {
    return current::FromString<KEY>(url_key);
  }
  template <typename KEY>
  static std::string ComposeURLKey(const KEY& key) {
    return FormatURLKey(current::ToString(key));
  }
  template <typename RECORD>
  static auto ExtractOrComposeKey(const RECORD& entry)
      -> decltype(current::storage::sfinae::GetKey(std::declval<RECORD>())) {
    return current::storage::sfinae::GetKey(entry);
  }
};

template <>
struct FieldTypeDependentImpl<semantics::primary_key::RowCol> {
  using url_key_t = std::pair<std::string, std::string>;

  template <typename F_WITH, typename F_WITHOUT>
  static void CallWithOrWithoutKeyFromURL(Request&& request_rref, F_WITH&& with, F_WITHOUT&& without) {
    // TODO(dkorolev): `?1=...&2=...`, `?L=...&R=...`, etc. -- make the code more generic.
    if (request_rref.url_path_args.size() == 2u) {
      with(std::move(request_rref),
           std::make_pair(request_rref.url_path_args[0], request_rref.url_path_args[1]));
    } else if (request_rref.url.query.has("key1") && request_rref.url.query.has("key2")) {
      with(std::move(request_rref),
           std::make_pair(request_rref.url.query["key1"], request_rref.url.query["key2"]));
    } else if (request_rref.url.query.has("row") && request_rref.url.query.has("col")) {
      with(std::move(request_rref),
           std::make_pair(request_rref.url.query["row"], request_rref.url.query["col"]));
    } else if (request_rref.url.query.has("1") && request_rref.url.query.has("2")) {
      with(std::move(request_rref), std::make_pair(request_rref.url.query["1"], request_rref.url.query["2"]));
    } else {
      without(std::move(request_rref));
    }
  }
  static std::string FormatURLKey(const url_key_t& key) {
    // TODO(dkorolev): Perhaps return "?row=...&col=" in case the key contains slashes?
    return key.first + '/' + key.second;
  }
  template <typename KEY, typename URL_KEY>
  static KEY ParseURLKey(const URL_KEY& url_key) {
    return KEY(current::FromString<current::storage::sfinae::entry_row_t<KEY>>(url_key.first),
               current::FromString<current::storage::sfinae::entry_col_t<KEY>>(url_key.second));
  }
  template <typename KEY>
  static std::string ComposeURLKey(const KEY& key) {
    return FormatURLKey(std::make_pair(current::ToString(current::storage::sfinae::GetRow(key)),
                                       current::ToString(current::storage::sfinae::GetCol(key))));
  }

  // TODO(dkorolev): `RECORD` -> `ENTRY` and `decltype` -> `key_t` or `sfinae::...`?
  template <typename RECORD>
  static auto ExtractOrComposeKey(const RECORD& entry)
      -> std::pair<decltype(current::storage::sfinae::GetRow(std::declval<RECORD>())),
                   decltype(current::storage::sfinae::GetCol(std::declval<RECORD>()))> {
    return std::make_pair(current::storage::sfinae::GetRow(entry), current::storage::sfinae::GetCol(entry));
  }
};

template <typename PRIMARY_KEY_TYPE>
struct FieldTypeDependent : FieldTypeDependentImpl<PRIMARY_KEY_TYPE> {
  template <typename F>
  static void CallWithKeyFromURL(Request&& request_rref, F&& next_with_key) {
    FieldTypeDependentImpl<PRIMARY_KEY_TYPE>::CallWithOrWithoutKeyFromURL(
        std::move(request_rref),
        std::forward<F>(next_with_key),
        [](Request&& proxied_request_rref) {
          proxied_request_rref("Need resource key in the URL.\n", HTTPResponseCode.BadRequest);
        });
  }

  template <typename F>
  static void CallWithOptionalKeyFromURL(Request&& request_rref, F&& next) {
    FieldTypeDependentImpl<PRIMARY_KEY_TYPE>::CallWithOrWithoutKeyFromURL(
        std::move(request_rref),
        std::forward<F>(next),
        [&next](Request&& proxied_request_rref) { next(std::move(proxied_request_rref), nullptr); });
  }
};

template <typename KEY>
using key_type_dependent_t = FieldTypeDependent<KEY>;

template <typename FIELD>
using field_type_dependent_t = key_type_dependent_t<typename FIELD::semantics_t::primary_key_t>;
// TODO(dkorolev): The whole `FieldTypeDependentImpl` section above to be moved to `semantics.h`.

template <typename STORAGE>
struct RESTfulGenericInput {
  using STORAGE_TYPE = STORAGE;
  STORAGE& storage;
  const std::string restful_url_prefix;

  explicit RESTfulGenericInput(STORAGE& storage) : storage(storage) {}
  RESTfulGenericInput(STORAGE& storage, const std::string& restful_url_prefix)
      : storage(storage), restful_url_prefix(restful_url_prefix) {}
  RESTfulGenericInput(const RESTfulGenericInput&) = default;
  RESTfulGenericInput(RESTfulGenericInput&&) = default;
};

template <typename STORAGE>
struct RESTfulRegisterTopLevelInput : RESTfulGenericInput<STORAGE> {
  const int port;
  HTTPRoutesScope& scope;
  const std::vector<std::string> field_names;
  const std::string route_prefix;
  std::atomic_bool& up_status;

  RESTfulRegisterTopLevelInput(STORAGE& storage,
                               const std::string& restful_url_prefix,
                               int port,
                               HTTPRoutesScope& scope,
                               const std::vector<std::string>& field_names,
                               const std::string& route_prefix,
                               std::atomic_bool& up_status)
      : RESTfulGenericInput<STORAGE>(storage, restful_url_prefix),
        port(port),
        scope(scope),
        field_names(field_names),
        route_prefix(route_prefix),
        up_status(up_status) {}
  RESTfulRegisterTopLevelInput(const RESTfulGenericInput<STORAGE>& input,
                               int port,
                               HTTPRoutesScope& scope,
                               const std::vector<std::string>& field_names,
                               const std::string& route_prefix,
                               std::atomic_bool& up_status)
      : RESTfulGenericInput<STORAGE>(input),
        port(port),
        scope(scope),
        field_names(field_names),
        route_prefix(route_prefix),
        up_status(up_status) {}
  RESTfulRegisterTopLevelInput(RESTfulGenericInput<STORAGE>&& input,
                               int port,
                               HTTPRoutesScope& scope,
                               const std::vector<std::string>& field_names,
                               const std::string& route_prefix,
                               std::atomic_bool& up_status)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        port(port),
        scope(scope),
        field_names(field_names),
        route_prefix(route_prefix),
        up_status(up_status) {}
};

template <typename STORAGE, typename FIELD>
struct RESTfulGETInput : RESTfulGenericInput<STORAGE> {
  using field_t = FIELD;
  using key_completeness_t = semantics::key_completeness::FullKey;

  using immutable_fields_t = ImmutableFields<STORAGE>;
  using url_key_t = typename field_t::semantics_t::url_key_t;

  immutable_fields_t fields;
  const field_t& field;
  const std::string field_name;
  Optional<url_key_t> get_url_key;
  const StorageRole role;
  const bool export_requested;

  RESTfulGETInput(const RESTfulGenericInput<STORAGE>& input,
                  immutable_fields_t fields,
                  const field_t& field,
                  const std::string& field_name,
                  const Optional<url_key_t>& get_url_key,
                  const StorageRole role,
                  const bool export_requested)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        get_url_key(get_url_key),
        role(role),
        export_requested(export_requested) {}
  RESTfulGETInput(RESTfulGenericInput<STORAGE>&& input,
                  immutable_fields_t fields,
                  const field_t& field,
                  const std::string& field_name,
                  const Optional<url_key_t>& get_url_key,
                  const StorageRole role,
                  const bool export_requested)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        get_url_key(get_url_key),
        role(role),
        export_requested(export_requested) {}
};

// A dedicated "GETInput" for the GETs over row or col of a matrix container.
template <typename STORAGE, typename INCOMPLETE_KEY_TYPE, typename FIELD>
struct RESTfulGETRowColInput : RESTfulGenericInput<STORAGE> {
  static_assert(std::is_same<INCOMPLETE_KEY_TYPE, semantics::key_completeness::PartialRowKey>::value ||
                    std::is_same<INCOMPLETE_KEY_TYPE, semantics::key_completeness::PartialColKey>::value,
                "");

  using field_t = FIELD;
  using key_completeness_t = INCOMPLETE_KEY_TYPE;
  using immutable_fields_t = ImmutableFields<STORAGE>;

  immutable_fields_t fields;
  const field_t& field;
  const std::string field_name;
  Optional<std::string> rowcol_get_url_key;

  RESTfulGETRowColInput(const RESTfulGenericInput<STORAGE>& input,
                        immutable_fields_t fields,
                        const field_t& field,
                        const std::string& field_name,
                        const Optional<std::string>& rowcol_get_url_key)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        rowcol_get_url_key(rowcol_get_url_key) {}
  RESTfulGETRowColInput(RESTfulGenericInput<STORAGE>&& input,
                        immutable_fields_t fields,
                        const field_t& field,
                        const std::string& field_name,
                        const Optional<std::string>& rowcol_get_url_key)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        rowcol_get_url_key(rowcol_get_url_key) {}
};

template <typename STORAGE, typename FIELD, typename ENTRY>
struct RESTfulPOSTInput : RESTfulGenericInput<STORAGE> {
  using field_t = FIELD;
  using mutable_fields_t = MutableFields<STORAGE>;
  mutable_fields_t fields;
  field_t& field;
  const std::string field_name;
  ENTRY& entry;

  RESTfulPOSTInput(const RESTfulGenericInput<STORAGE>& input,
                   mutable_fields_t fields,
                   field_t& field,
                   const std::string& field_name,
                   ENTRY& entry)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        entry(entry) {}
  RESTfulPOSTInput(RESTfulGenericInput<STORAGE>&& input,
                   mutable_fields_t fields,
                   field_t& field,
                   const std::string& field_name,
                   ENTRY& entry)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        entry(entry) {}
};

template <typename STORAGE, typename FIELD, typename ENTRY, typename KEY>
struct RESTfulPUTInput : RESTfulGenericInput<STORAGE> {
  using field_t = FIELD;
  using mutable_fields_t = MutableFields<STORAGE>;
  mutable_fields_t fields;
  field_t& field;
  const std::string field_name;
  const KEY& put_key;
  const ENTRY& entry;
  const KEY& entry_key;

  RESTfulPUTInput(const RESTfulGenericInput<STORAGE>& input,
                  mutable_fields_t fields,
                  field_t& field,
                  const std::string& field_name,
                  const KEY& put_key,
                  const ENTRY& entry,
                  const KEY& entry_key)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        put_key(put_key),
        entry(entry),
        entry_key(entry_key) {}
  RESTfulPUTInput(RESTfulGenericInput<STORAGE>&& input,
                  mutable_fields_t fields,
                  field_t& field,
                  const std::string& field_name,
                  const KEY& put_key,
                  const ENTRY& entry,
                  const KEY& entry_key)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        put_key(put_key),
        entry(entry),
        entry_key(entry_key) {}
};

template <typename STORAGE, typename FIELD, typename KEY>
struct RESTfulDELETEInput : RESTfulGenericInput<STORAGE> {
  using field_t = FIELD;
  using mutable_fields_t = MutableFields<STORAGE>;
  mutable_fields_t fields;
  field_t& field;
  const std::string field_name;
  const KEY& delete_key;

  RESTfulDELETEInput(const RESTfulGenericInput<STORAGE>& input,
                     mutable_fields_t fields,
                     field_t& field,
                     const std::string& field_name,
                     const KEY& delete_key)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        delete_key(delete_key) {}
  RESTfulDELETEInput(RESTfulGenericInput<STORAGE>&& input,
                     mutable_fields_t fields,
                     field_t& field,
                     const std::string& field_name,
                     const KEY& delete_key)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        delete_key(delete_key) {}
};

// Generic wrapper to have per-`Row` and per-`Col` code collapsed into one snippet.
// Also make sure all matrix containers -- ManyToMany, OneToMany, OneToOne -- can be iterated
// in semantically identical ways: assuming an iterable range of records per row/col.
template <typename>
struct MatrixContainerProxy;

template <>
struct MatrixContainerProxy<semantics::key_completeness::PartialRowKey> {
  template <typename FIELD>
  using field_outer_key_t = typename current::decay<FIELD>::row_t;

  template <typename FIELD>
  using field_inner_key_t = typename current::decay<FIELD>::col_t;

  template <typename ENTRY>
  using entry_outer_key_t = current::storage::sfinae::entry_row_t<ENTRY>;

  template <typename FIELD>
  using outer_accessor_t = typename current::decay<FIELD>::rows_outer_accessor_t;

  template <typename FIELD, typename ROW>
  static GenericMapAccessor<typename current::decay<FIELD>::row_elements_map_t> RowOrCol(FIELD&& field,
                                                                                         ROW&& row) {
    return field.Row(std::forward<ROW>(row));
  }

  template <typename FIELD>
  static outer_accessor_t<FIELD> RowsOrCols(FIELD&& field) {
    return field.Rows();
  }

  template <typename FIELD, typename ROW>
  static ImmutableOptional<typename current::decay<FIELD>::entry_t> GetEntryFromRowOrCol(FIELD&& field,
                                                                                         ROW&& row) {
    return field.GetEntryFromRow(std::forward<ROW>(row));
  }

  static const std::string& PartialKeySuffix() {
    static std::string suffix = "1";
    return suffix;
  }
};

template <>
struct MatrixContainerProxy<semantics::key_completeness::PartialColKey> {
  template <typename FIELD>
  using field_outer_key_t = typename current::decay<FIELD>::col_t;

  template <typename FIELD>
  using field_inner_key_t = typename current::decay<FIELD>::row_t;

  template <typename ENTRY>
  using entry_outer_key_t = current::storage::sfinae::entry_col_t<ENTRY>;

  template <typename FIELD>
  using outer_accessor_t = typename current::decay<FIELD>::cols_outer_accessor_t;

  template <typename FIELD, typename COL>
  static GenericMapAccessor<typename current::decay<FIELD>::col_elements_map_t> RowOrCol(FIELD&& field,
                                                                                         COL&& col) {
    return field.Col(std::forward<COL>(col));
  }

  template <typename FIELD>
  static outer_accessor_t<FIELD> RowsOrCols(FIELD&& field) {
    return field.Cols();
  }

  template <typename FIELD, typename COL>
  static ImmutableOptional<typename current::decay<FIELD>::entry_t> GetEntryFromRowOrCol(FIELD&& field,
                                                                                         COL&& col) {
    return field.GetEntryFromCol(std::forward<COL>(col));
  }

  static const std::string& PartialKeySuffix() {
    static std::string suffix = "2";
    return suffix;
  }
};

// A special type to wrap the iterator passed into matrix row/col metadata rendering.
// The `OUTER_KEY` type is the type of the row or col respectively, when browsing rows or cols.
// Its value is accessible as `iterator.key()`.
template <typename OUTER_KEY, typename ITERATOR>
struct SingleElementContainer {
  using outer_key_t = OUTER_KEY;
  using iterator_t = ITERATOR;
  outer_key_t outer_key;
  iterator_t iterator;
  SingleElementContainer() = default;
  SingleElementContainer(current::copy_free<outer_key_t> outer_key, iterator_t iterator)
      : outer_key(outer_key), iterator(iterator) {}
  // This method can be avoided, but it's kept for clarity and cleaniness of the user code. -- D.K.
  size_t TotalElementsForHypermediaCollectionView() const { return 1u; }
};

template <typename, typename>
struct GenericMatrixIteratorImplSelector;

template <typename PARTIAL_KEY_TYPE>
struct GenericMatrixIteratorImplSelector<PARTIAL_KEY_TYPE, semantics::matrix_dimension_type::IterableRange> {
  using Impl = MatrixContainerProxy<PARTIAL_KEY_TYPE>;
};

template <typename PARTIAL_KEY_TYPE>
struct GenericMatrixIteratorImplSelector<PARTIAL_KEY_TYPE, semantics::matrix_dimension_type::SingleElement> {
  using Proxy = MatrixContainerProxy<PARTIAL_KEY_TYPE>;

  template <typename ENTRY>
  struct SingleElementInnerAccessor {
    // This magic as ImmutableOptional<> can't be copied over, and I'm too lazy
    // to get into introducing move constructors everywhere here. -- D.K.
    Optional<ENTRY> entry;
    SingleElementInnerAccessor(const ImmutableOptional<ENTRY> input_entry) {
      if (Exists(input_entry)) {
        entry = Value(input_entry);
      }
    }

    bool Empty() const { return !Exists(entry); }
    size_t Size() const { return Exists(entry) ? 1u : 0u; }
    struct SingleElementInnerIterator {
      using value_t = ENTRY;
      const Optional<ENTRY>& entry;
      size_t i;
      SingleElementInnerIterator(const Optional<ENTRY>& entry, size_t i) : entry(entry), i(i) {}
      ENTRY operator*() const { return Value(entry); }
      bool operator==(const SingleElementInnerIterator& rhs) { return i == rhs.i; }
      bool operator!=(const SingleElementInnerIterator& rhs) { return !operator==(rhs); }
      void operator++() { ++i; }
    };

    SingleElementInnerIterator begin() const { return SingleElementInnerIterator(entry, 0u); }
    SingleElementInnerIterator end() const { return SingleElementInnerIterator(entry, Size()); }
  };

  template <typename FIELD, typename OUTER_KEY, typename ENTRY>
  struct SingleElementOuterAccessor {
    using outer_accessor_t = typename MatrixContainerProxy<PARTIAL_KEY_TYPE>::template outer_accessor_t<FIELD>;
    using outer_iterator_t = typename outer_accessor_t::const_iterator;
    outer_accessor_t accessor;
    SingleElementOuterAccessor(outer_accessor_t accessor) : accessor(accessor) {}
    bool Empty() const { return accessor.Empty(); }
    size_t Size() const { return accessor.Size(); }
    struct SingleElementOuterIterator final {
      outer_iterator_t iterator;
      SingleElementOuterIterator(outer_iterator_t iterator) : iterator(iterator) {}
      void operator++() { ++iterator; }
      bool operator==(const SingleElementOuterIterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const SingleElementOuterIterator& rhs) const { return !operator==(rhs); }
      current::copy_free<OUTER_KEY> OuterKeyForPartialHypermediaCollectionView() const {
        return iterator.key();
      }
      using value_t = SingleElementContainer<OUTER_KEY, outer_iterator_t>;
      // DIMA_FIXME: Remove all `has_range_element_t`, and likely remove/rename all `range_element_t`.
      // DIMA_FIXME: Remove all `value_t`, or change them into some `c5t_value_t` to eliminate ambiguity.
      void has_range_element_t() {}
      using range_element_t = SingleElementContainer<OUTER_KEY, outer_iterator_t>;
      const range_element_t operator*() const {
        // TODO(dkorolev): DIMA_FIXME: There's no reason the 2nd parameter can't be `*iterator`.
        return range_element_t(iterator.key(), iterator);
      }
    };
    SingleElementOuterIterator begin() const { return SingleElementOuterIterator(accessor.begin()); }
    SingleElementOuterIterator end() const { return SingleElementOuterIterator(accessor.end()); }
  };

  struct Impl {
    template <typename FIELD, typename ROW_OR_COL>
    static SingleElementInnerAccessor<typename current::decay<FIELD>::entry_t> RowOrCol(
        FIELD&& field, ROW_OR_COL&& row_or_col) {
      return SingleElementInnerAccessor<typename current::decay<FIELD>::entry_t>(
          MatrixContainerProxy<PARTIAL_KEY_TYPE>::GetEntryFromRowOrCol(std::forward<FIELD>(field),
                                                                       std::forward<ROW_OR_COL>(row_or_col)));
    }

    template <typename FIELD>
    using outer_accessor_t =
        SingleElementOuterAccessor<current::decay<FIELD>,
                                   typename Proxy::template field_outer_key_t<current::decay<FIELD>>,
                                   typename current::decay<FIELD>::entry_t>;

    template <typename FIELD>
    static SingleElementOuterAccessor<current::decay<FIELD>,
                                      typename Proxy::template field_outer_key_t<current::decay<FIELD>>,
                                      typename current::decay<current::decay<FIELD>>::entry_t>
    RowsOrCols(FIELD&& field) {
      return SingleElementOuterAccessor<current::decay<FIELD>,
                                        typename Proxy::template field_outer_key_t<current::decay<FIELD>>,
                                        typename current::decay<current::decay<FIELD>>::entry_t>(
          MatrixContainerProxy<PARTIAL_KEY_TYPE>::RowsOrCols(field));
    }
  };
};

template <typename PARTIAL_KEY, typename FIELD>
struct ExtractRowOrColDimensionType;

template <typename FIELD>
struct ExtractRowOrColDimensionType<semantics::key_completeness::PartialRowKey, FIELD> {
  using matrix_dimension_t = typename FIELD::row_dimension_t;
};

template <typename FIELD>
struct ExtractRowOrColDimensionType<semantics::key_completeness::PartialColKey, FIELD> {
  using matrix_dimension_t = typename FIELD::col_dimension_t;
};

template <typename PARTIAL_KEY, typename FIELD>
using GenericMatrixIterator = typename GenericMatrixIteratorImplSelector<
    PARTIAL_KEY,
    typename ExtractRowOrColDimensionType<PARTIAL_KEY, FIELD>::matrix_dimension_t>::Impl;

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_API_TYPES_H
