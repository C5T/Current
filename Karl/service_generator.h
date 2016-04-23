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

#ifndef KERL_SERVICE_GENERATOR_H
#define KERL_SERVICE_GENERATOR_H

#include "karl.h"

#include "service_schema.h"

#include <atomic>

#include "../Blocks/HTTP/api.h"

#include "../Sherlock/sherlock.h"

#include "../Bricks/time/chrono.h"

namespace karl_unittest {

class ServiceGenerator final {
 public:
  ServiceGenerator(int port, std::chrono::milliseconds sleep_between_numbers = std::chrono::milliseconds(10))
      : current_value_(1),
        stream_(current::sherlock::Stream<Number>()),
        http_scope_(HTTP(port).Register("/numbers", stream_)),
        sleep_between_numbers_(sleep_between_numbers),
        destructing_(false),
        thread_([this]() { Thread(); }) {}

  ~ServiceGenerator() {
    destructing_ = true;
    thread_.join();
  }

 private:
  void Thread() {
    while (!destructing_) {
#ifdef CURRENT_MOCK_TIME
      current::time::SetNow(std::chrono::microseconds(current_value_ * 1000 * 1000));
#endif
      stream_.Publish(Number(current_value_));
      ++current_value_;
      std::this_thread::sleep_for(sleep_between_numbers_);
    }
  }

 private:
  int current_value_;
  current::sherlock::Stream<Number> stream_;
  HTTPRoutesScope http_scope_;
  const std::chrono::milliseconds sleep_between_numbers_;
  std::atomic_bool destructing_;
  std::thread thread_;
};

}  // namespace karl_unittest

#endif  // KARL_SERVICE_GENERATOR_H
