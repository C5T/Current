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

#ifndef KERL_SERVICE_FILTER_H
#define KERL_SERVICE_FILTER_H

#include "karl.h"

#include "service_schema.h"

#include "../Blocks/HTTP/api.h"

#include "../Sherlock/sherlock.h"

namespace karl_unittest {

class ServiceFilter final {
 public:
  ServiceFilter(int port, const std::string& source_annotated_numbers_stream)
      : source_annotated_numbers_stream_(source_annotated_numbers_stream),
        stream_(current::sherlock::Stream<Number>()),
        http_scope_(HTTP(port).Register("/filtered", stream_)),
        destructing_(false),
        thread_([this]() { Thread(); }) {}

  ~ServiceFilter() {
    destructing_ = true;
    thread_.join();
  }

 private:
  void Thread() {
    // Poor man's stream subscriber. -- D.K.
    // TODO(dkorolev) + TODO(mzhurovich): Revisit in Thailand as as coin the notion of `HTTPSherlockSusbcriber`.
    int index = 0;
    try {
      while (!destructing_) {
        const auto row =
            HTTP(GET(source_annotated_numbers_stream_ + "?i=" + current::ToString(index++) + "&n=1")).body;
        const auto split = current::strings::Split(row, '\t');
        assert(split.size() == 2u);
        auto number = ParseJSON<Number>(split[1]);
        assert(Exists(number.is_prime));
        if (Value(number.is_prime)) {
          stream_.Publish(number);
        }
      }
    } catch (current::net::NetworkException&) {
      // Ignore for the purposes of this test. -- D.K.
    }
  }

  const std::string source_annotated_numbers_stream_;
  current::sherlock::Stream<Number> stream_;
  HTTPRoutesScope http_scope_;
  std::atomic_bool destructing_;
  std::thread thread_;
};

}  // namespace karl_unittest

#endif  // KARL_SERVICE_FILTER_H
