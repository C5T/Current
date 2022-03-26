/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STORAGE_REST_SIMPLE_H
#define CURRENT_STORAGE_REST_SIMPLE_H

#include "structured.h"

namespace current {
namespace storage {
namespace rest {
namespace simple {

struct SimpleResponseFormatter {
  struct Context {};

  template <typename ENTRY>
  static Response BuildResponseForResource(const Context&,
                                           const std::string& url,
                                           const std::string& url_collection_unused_by_simple,
                                           const ENTRY& entry) {
    static_cast<void>(url_collection_unused_by_simple);
    return Response(SimpleRESTRecordResponse<ENTRY>(url, entry));
  }

  template <typename PARTICULAR_FIELD, typename ENTRY, typename INNER_HYPERMEDIA_TYPE, typename ITERABLE>
  static Response BuildResponseWithCollection(const Context&,
                                              const std::string& pagination_url,
                                              const std::string& collection_url,
                                              ITERABLE&& span) {
    SimpleRESTContainerResponse payload;
    payload.url = pagination_url;
    for (auto iterator = span.begin(); iterator != span.end(); ++iterator) {
      payload.data.emplace_back(collection_url + '/' + ComposeRESTfulKey<PARTICULAR_FIELD, ENTRY>(iterator));
    }
    return Response(payload);
  }
};

}  // namespace simple

using Simple = generic::Structured<simple::SimpleResponseFormatter>;

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_SIMPLE_H
