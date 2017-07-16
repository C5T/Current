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

#ifndef CURRENT_MIDICHLORIANS_SERVER_SCHEMA_H
#define CURRENT_MIDICHLORIANS_SERVER_SCHEMA_H

#include "../MidichloriansDataDictionary.h"

#include "../../bricks/template/typelist.h"
#include "../../Blocks/HTTP/request.h"
#include "../../typesystem/struct.h"

namespace current {
namespace midichlorians {
namespace server {

using current::midichlorians::ios::ios_events_t;
using current::midichlorians::web::web_events_t;

using ios_variant_t = Variant<ios_events_t>;
using web_variant_t = Variant<web_events_t>;
using event_variant_t = Variant<current::metaprogramming::TypeListCat<ios_events_t, web_events_t>>;

// clang-format off
CURRENT_STRUCT(LogEntryBase) {
  CURRENT_FIELD(server_us, std::chrono::microseconds, 0);
  CURRENT_DEFAULT_CONSTRUCTOR(LogEntryBase) {}
  CURRENT_CONSTRUCTOR(LogEntryBase)(std::chrono::microseconds us) : server_us(us) {}
};

CURRENT_STRUCT(TickLogEntry, LogEntryBase){
  CURRENT_DEFAULT_CONSTRUCTOR(TickLogEntry) {}
  CURRENT_CONSTRUCTOR(TickLogEntry)(std::chrono::microseconds us) : SUPER(us) {}
};

CURRENT_STRUCT(EventLogEntry, LogEntryBase) {
  CURRENT_FIELD(event, event_variant_t);
  CURRENT_DEFAULT_CONSTRUCTOR(EventLogEntry) {}
  CURRENT_CONSTRUCTOR(EventLogEntry)(std::chrono::microseconds us, web_variant_t&& event)
      : SUPER(us), event(std::move(event)) {}
  CURRENT_CONSTRUCTOR(EventLogEntry)(std::chrono::microseconds us, ios_variant_t&& event)
      : SUPER(us), event(std::move(event)) {}
};

CURRENT_STRUCT(UnparsableLogEntry, LogEntryBase) {
  CURRENT_FIELD(method, std::string);
  CURRENT_FIELD(headers, (std::map<std::string, std::string>));
  CURRENT_FIELD(cookies, std::string);
  CURRENT_FIELD(body, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(UnparsableLogEntry) {}
  CURRENT_CONSTRUCTOR(UnparsableLogEntry)(std::chrono::microseconds us, const current::http::Request& r)
      : SUPER(us), method(r.method), headers(r.headers.AsMap()), cookies(r.headers.CookiesAsString()), body(r.body) {}
};
// clang-format on

using log_entry_variant_t = Variant<TickLogEntry, EventLogEntry, UnparsableLogEntry>;

}  // namespace server
}  // namespace midichlorians
}  // namespace current

#endif  // CURRENT_MIDICHLORIANS_SERVER_SCHEMA_H
