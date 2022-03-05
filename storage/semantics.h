/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STORAGE_SEMANTICS_H
#define CURRENT_STORAGE_SEMANTICS_H

#include "../port.h"

#include "container/sfinae.h"

#include "../blocks/http/api.h"

namespace current {
namespace storage {

// DIMA_FIXME, TODO(dkorolev): Many of these helper empty structs should be enum classes instead.
// DIMA_FIXME, ref. https://dimakorolev.quora.com/C++-Template-Metaprogramming

// Helper empty classes for call dispatching, REST and generic cross-container access.
namespace semantics {

namespace rest {
struct RESTWithSingleKey {};
struct RESTWithPairedKey {};
}  // namespace rest

namespace primary_key {
struct Key {};
struct RowCol {};
}  // namespace primary_key

namespace matrix_dimension_type {
struct IterableRange {};
struct SingleElement {};
}  // namespace matrix_dimension_type

struct Dictionary {
  using url_key_t = std::string;
  using primary_key_t = primary_key::Key;
  using rest_endpoints_schema_t = rest::RESTWithSingleKey;
};
struct ManyToMany {
  using url_key_t = std::pair<std::string, std::string>;
  using primary_key_t = primary_key::RowCol;
  using rest_endpoints_schema_t = rest::RESTWithPairedKey;
  using row_dimension_t = matrix_dimension_type::IterableRange;
  using col_dimension_t = matrix_dimension_type::IterableRange;
};
struct OneToMany {
  using url_key_t = std::pair<std::string, std::string>;
  using primary_key_t = primary_key::RowCol;
  using rest_endpoints_schema_t = rest::RESTWithPairedKey;
  using row_dimension_t = matrix_dimension_type::IterableRange;
  using col_dimension_t = matrix_dimension_type::SingleElement;
};
struct OneToOne {
  using url_key_t = std::pair<std::string, std::string>;
  using primary_key_t = primary_key::RowCol;
  using rest_endpoints_schema_t = rest::RESTWithPairedKey;
  using row_dimension_t = matrix_dimension_type::SingleElement;
  using col_dimension_t = matrix_dimension_type::SingleElement;
};

namespace key_completeness {
struct DictionaryOrMatrixCompleteKey {};
struct MatrixHalfKey {};
struct FullKey {
  using completeness_family_t = DictionaryOrMatrixCompleteKey;
};
struct PartialRowKey {
  using completeness_family_t = MatrixHalfKey;
};
struct PartialColKey {
  using completeness_family_t = MatrixHalfKey;
};
}  // namespace key_completeness

// TODO(dkorolev): These actually belong to REST, not to storage/container semantics.
namespace rest {
namespace operation {
struct OnDictionaryTopLevel {
  using key_completeness_t = semantics::key_completeness::FullKey;
  using top_level_iterating_key_t = semantics::primary_key::Key;
};
struct OnMatrixTopLevel {
  using key_completeness_t = semantics::key_completeness::FullKey;
  using top_level_iterating_key_t = semantics::primary_key::RowCol;
};
struct OnMatrixRow {
  using key_completeness_t = semantics::key_completeness::PartialRowKey;
};
struct OnMatrixCol {
  using key_completeness_t = semantics::key_completeness::PartialColKey;
};

template <typename>
struct TopLevelOperationSelector;

template <>
struct TopLevelOperationSelector<primary_key::Key> {
  using type = OnDictionaryTopLevel;
};

template <>
struct TopLevelOperationSelector<primary_key::RowCol> {
  using type = OnMatrixTopLevel;
};

template <typename FIELD>
using top_level_operation_for_field_t =
    typename TopLevelOperationSelector<typename FIELD::semantics_t::primary_key_t>::type;

}  // namespace operation
}  // namespace rest

}  // namespace semantics

// TODO(dkorolev): `FieldTypeDependentImpl` as `FieldKeyTypeDependentImpl` from `api_types.h` to be moved here.

}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_SEMANTICS_H
