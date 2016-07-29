/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

// Hypermedia: A rather hacky solution for Hypermedia REST API supporting:
// * Rich JSON format (top-level `url_*` fields, and actual data in `data`.)
// * Poor man's stateless "pagination" through collections and collection "slices" (rows/cols of matrices).
// * Full and brief fields sets.

#ifndef CURRENT_STORAGE_REST_HYPERMEDIA_H
#define CURRENT_STORAGE_REST_HYPERMEDIA_H

#include "simple.h"

namespace current {
namespace storage {
namespace rest {

namespace hypermedia {

template <typename ENTRY, typename VALUE>
struct PopulateCollectionRecord;  // Intentionally left empty to make sure each case is covered. -- D.K.

template <typename ENTRY>
struct PopulateCollectionRecord<ENTRY, ENTRY> {
  template <typename OUTPUT, typename ITERATOR>
  static void DoIt(OUTPUT& output, ITERATOR&& iterator) {
    output = *iterator;
  }
};

template <typename ENTRY, typename K, typename I>
struct PopulateCollectionRecord<ENTRY, SingleElementContainer<K, I>> {
  template <typename OUTPUT, typename ITERATOR>
  static void DoIt(OUTPUT& output, ITERATOR&& iterator) {
    const auto& collection = *iterator;
    // Must be extactly one, but just to look nice.
    output.total = static_cast<int>(collection.TotalElementsForHypermediaCollectionView());
    output.preview.push_back(*collection.iterator);  // Exactly one.
  }
};

template <typename ENTRY, typename MAP>
struct PopulateCollectionRecord<ENTRY, current::GenericMapAccessor<MAP>> {
  template <typename OUTPUT, typename ITERATOR>
  static void DoIt(OUTPUT& output, ITERATOR&& iterator) {
    output.total = static_cast<int64_t>(iterator.TotalElementsForHypermediaCollectionView());
    size_t i = 0;
    for (const auto& e : (*iterator)) {
      output.preview.push_back(e);
      ++i;
      if (i >= 3u) {
        break;
      }
    }
  }
};

template <typename PARTICULAR_FIELD,
          typename ENTRY,
          typename INNER_HYPERMEDIA_TYPE,
          typename T,
          typename ITERATOR>
inline HypermediaRESTCollectionRecord<T> PrepareCollectionRecord(const std::string& url_directory,
                                                                 ITERATOR&& iterator) {
  HypermediaRESTCollectionRecord<T> record;

  const std::string key_as_url_string = ComposeRESTfulKey<PARTICULAR_FIELD, ENTRY>(iterator);
  record.url = url_directory + '/' + key_as_url_string;

  PopulateCollectionRecord<ENTRY, typename current::decay<typename current::decay<ITERATOR>::value_t>>::DoIt(
      record.data, std::forward<ITERATOR>(iterator));

  return record;
}

struct HypermediaResponseFormatter {
  // TODO(dkorolev): We could move to per-HTTP-VERB context type as it's high performance time.
  struct Context {
    // For per-record view, whether a full or brief format should be used.
    bool brief = false;

    // For poor man's pagination when viewing the collection.
    mutable uint64_t query_i = 0u;
    mutable uint64_t query_n = 10u;  // Default page size.
  };

  template <typename ENTRY>
  static Response BuildResponseForResource(const Context& context,
                                           const std::string& url,
                                           const std::string& url_collection,
                                           const ENTRY& entry) {
    using brief_t = sfinae::brief_of_t<ENTRY>;
    if (std::is_same<ENTRY, brief_t>::value) {
      return Response(HypermediaRESTSingleRecordResponse<ENTRY>(url, url_collection, entry));
    } else {
      if (!context.brief) {
        return Response(HypermediaRESTSingleRecordResponse<ENTRY>(url, url_collection, entry));
      } else {
        return Response(HypermediaRESTSingleRecordResponse<brief_t>(
            url, url_collection, static_cast<const brief_t&>(entry)));
      }
    }
  }

  template <typename PARTICULAR_FIELD, typename ENTRY, typename INNER_HYPERMEDIA_TYPE, typename ITERABLE>
  static Response BuildResponseWithCollection(const Context& context, const std::string& url, ITERABLE&& span) {
    using inner_element_t = sfinae::brief_of_t<INNER_HYPERMEDIA_TYPE>;
    using collection_element_t = HypermediaRESTCollectionRecord<inner_element_t>;

    const auto GenPageURL = [&url](uint64_t url_i, uint64_t url_n) {
      return url + "?i=" + current::ToString(url_i) + "&n=" + current::ToString(url_n);
    };

    HypermediaRESTCollectionResponse<collection_element_t> response;
    response.url_directory = url;

    // Poor man's pagination.
    const size_t total = span.Size();
    bool has_previous_page = false;
    bool has_next_page = false;
    uint64_t current_index = 0;
    for (auto iterator = span.begin(); iterator != span.end(); ++iterator) {
      // NOTE(dkorolev): This `iterator` can be of more than three different kinds, among which are:
      // 1) container/many_to_many.h. ManyToMany::OuterAccessor::OuterIterator
      // 2) container/one_to_many.h, OneToMany::RowsAccessor::RowsIterator
      // 3) api_types.h, GenericMatrixIteratorImplSelector<*, SE>::SWEOuterAccessor::SEOuterIterator,
      //    where SE stands for SingleElement.
      // 4) GenericMapAccessor<>.
      // To keep the generic code generic, it's accesses as `iterator`, not via a range-based loop.
      if (current_index >= context.query_i && current_index < context.query_i + context.query_n) {
        response.data.push_back(
            PrepareCollectionRecord<PARTICULAR_FIELD, ENTRY, INNER_HYPERMEDIA_TYPE, inner_element_t>(url,
                                                                                                     iterator));
      } else if (current_index < context.query_i) {
        has_previous_page = true;
      } else if (current_index >= context.query_i + context.query_n) {
        has_next_page = true;
        break;
      }
      ++current_index;
    }

    if (context.query_i > total) {
      context.query_i = total;
    }
    response.url = GenPageURL(context.query_i, context.query_n);
    response.i = context.query_i;
    response.n = std::min(context.query_n, total - context.query_i);
    response.total = total;
    if (has_previous_page) {
      response.url_previous_page = GenPageURL(
          context.query_i >= context.query_n ? context.query_i - context.query_n : 0, context.query_n);
    }
    if (has_next_page) {
      response.url_next_page =
          GenPageURL(context.query_i + context.query_n * 2 > total ? total - context.query_n
                                                                   : context.query_i + context.query_n,
                     context.query_n);
    }

    return Response(response, HTTPResponseCode.OK);
  }
};

}  // namespace current::storage::rest::hypermedia

struct Hypermedia : generic::Structured<hypermedia::HypermediaResponseFormatter> {
  using STRUCTURED = generic::Structured<hypermedia::HypermediaResponseFormatter>;

  template <class HTTP_VERB, typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler
      : STRUCTURED::RESTfulDataHandler<HTTP_VERB, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {};

  template <typename STORAGE, typename ENTRY>
  using RESTfulSchemaHandler = STRUCTURED::RESTfulSchemaHandler<STORAGE, ENTRY>;

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY>
      : STRUCTURED::RESTfulDataHandler<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    using SUPER_GET_HANDLER_GENERATOR =
        STRUCTURED::RESTfulDataHandler<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY>;

    template <typename F>
    void Enter(Request request, F&& next) {
      auto& context = SUPER_GET_HANDLER_GENERATOR::context;

      const auto& q = request.url.query;
      context.brief = ((q["fields"] == "brief") || q.has("brief")) && !q.has("full");
      context.query_i = current::FromString<uint64_t>(q.get("i", current::ToString(context.query_i)));
      context.query_n = current::FromString<uint64_t>(q.get("n", current::ToString(context.query_n)));

      SUPER_GET_HANDLER_GENERATOR::Enter(std::move(request), std::forward<F>(next));
    }
  };
};

}  // namespace current::storage::rest
}  // namespace current::storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_HYPERMEDIA_H
