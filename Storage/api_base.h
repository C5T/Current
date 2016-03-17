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

#ifndef CURRENT_STORAGE_API_BASE_H
#define CURRENT_STORAGE_API_BASE_H

#include "storage.h"

namespace current {
namespace storage {
namespace rest {

template <typename STORAGE>
struct RESTfulGenericInput {
  STORAGE& storage;
  const std::string restful_url_prefix;
  const std::string data_url_component;

  explicit RESTfulGenericInput(STORAGE& storage) : storage(storage) {}
  RESTfulGenericInput(STORAGE& storage,
                      const std::string& restful_url_prefix,
                      const std::string& data_url_component)
      : storage(storage), restful_url_prefix(restful_url_prefix), data_url_component(data_url_component) {}
  RESTfulGenericInput(const RESTfulGenericInput&) = default;
  RESTfulGenericInput(RESTfulGenericInput&&) = default;
};

template <typename STORAGE, typename FIELD>
struct RESTfulGETInput : RESTfulGenericInput<STORAGE> {
  using T_ALL_FIELDS = MutableFields<STORAGE>;
  T_ALL_FIELDS fields;
  const FIELD& field;
  const std::string field_name;
  const std::string url_key;

  RESTfulGETInput(const RESTfulGenericInput<STORAGE>& input,
                  T_ALL_FIELDS fields,
                  const FIELD& field,
                  const std::string& field_name,
                  const std::string& url_key)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        url_key(url_key) {}
  RESTfulGETInput(RESTfulGenericInput<STORAGE>&& input,
                  T_ALL_FIELDS fields,
                  const FIELD& field,
                  const std::string& field_name,
                  const std::string& url_key)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        url_key(url_key) {}
};

template <typename STORAGE, typename FIELD, typename ENTRY>
struct RESTfulPOSTInput : RESTfulGenericInput<STORAGE> {
  using T_ALL_FIELDS = MutableFields<STORAGE>;
  T_ALL_FIELDS fields;
  FIELD& field;
  const std::string field_name;
  ENTRY& entry;

  RESTfulPOSTInput(const RESTfulGenericInput<STORAGE>& input,
                   T_ALL_FIELDS fields,
                   FIELD& field,
                   const std::string& field_name,
                   ENTRY& entry)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        entry(entry) {}
  RESTfulPOSTInput(RESTfulGenericInput<STORAGE>&& input,
                   T_ALL_FIELDS fields,
                   FIELD& field,
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
  using T_ALL_FIELDS = MutableFields<STORAGE>;
  T_ALL_FIELDS fields;
  FIELD& field;
  const std::string field_name;
  const KEY& url_key;
  const ENTRY& entry;
  const KEY& entry_key;

  RESTfulPUTInput(const RESTfulGenericInput<STORAGE>& input,
                  T_ALL_FIELDS fields,
                  FIELD& field,
                  const std::string& field_name,
                  const KEY& url_key,
                  const ENTRY& entry,
                  const KEY& entry_key)
      : RESTfulGenericInput<STORAGE>(input),
        fields(fields),
        field(field),
        field_name(field_name),
        url_key(url_key),
        entry(entry),
        entry_key(entry_key) {}
  RESTfulPUTInput(RESTfulGenericInput<STORAGE>&& input,
                  T_ALL_FIELDS fields,
                  FIELD& field,
                  const std::string& field_name,
                  const KEY& url_key,
                  const ENTRY& entry,
                  const KEY& entry_key)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        url_key(url_key),
        entry(entry),
        entry_key(entry_key) {}
};

template <typename STORAGE, typename FIELD, typename KEY>
struct RESTfulDELETEInput : RESTfulGenericInput<STORAGE> {
  using T_ALL_FIELDS = MutableFields<STORAGE>;
  T_ALL_FIELDS fields;
  FIELD& field;
  const std::string field_name;
  const KEY& key;

  RESTfulDELETEInput(const RESTfulGenericInput<STORAGE>& input,
                     T_ALL_FIELDS fields,
                     FIELD& field,
                     const std::string& field_name,
                     const KEY& key)
      : RESTfulGenericInput<STORAGE>(input), fields(fields), field(field), field_name(field_name), key(key) {}
  RESTfulDELETEInput(RESTfulGenericInput<STORAGE>&& input,
                     T_ALL_FIELDS fields,
                     FIELD& field,
                     const std::string& field_name,
                     const KEY& key)
      : RESTfulGenericInput<STORAGE>(std::move(input)),
        fields(fields),
        field(field),
        field_name(field_name),
        key(key) {}
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_API_BASE_H
