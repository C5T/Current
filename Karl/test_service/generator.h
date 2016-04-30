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

#ifndef KARL_TEST_SERVICE_GENERATOR_H
#define KARL_TEST_SERVICE_GENERATOR_H

#include "../../port.h"

#include <atomic>

#include "../karl.h"
#include "../claire.h"

#include "schema.h"

#include "../../Blocks/HTTP/api.h"

#include "../../Sherlock/sherlock.h"

#include "../../Bricks/time/chrono.h"

namespace karl_unittest {

class ServiceGenerator final {
 public:
  ServiceGenerator(uint16_t port,
                   std::chrono::microseconds sleep_between_numbers,
                   const current::karl::Locator& karl)
      : current_value_(0),
        stream_(current::sherlock::Stream<Number>()),
        http_scope_(HTTP(port).Register("/numbers", stream_)),
        sleep_between_numbers_(sleep_between_numbers),
        destructing_(false),
        thread_([this]() { Thread(); }),
        claire_(karl, "generator", port) {
    const auto status_reporter = [this]() -> current::karl::DefaultClaireServiceStatus {
      current::karl::DefaultClaireServiceStatus user;
      user.message = "Up and running!";
      user.details["i"] = current::ToString(current_value_.load());
      return user;
    };
#ifdef CURRENT_MOCK_TIME
    // In unit test mode, wait for Karl's response and callback, and fail if Karl is not available.
    claire_.Register(status_reporter, true);
#else
    // In example "production" mode just start regular keepalives.
    claire_.Register(status_reporter);
#endif
  }

  ~ServiceGenerator() {
    destructing_ = true;
    thread_.join();
  }

 private:
  void Thread() {
    while (!destructing_) {
#ifdef CURRENT_MOCK_TIME
      current::time::SetNow(std::chrono::microseconds(current_value_ * 1000ull * 1000ull),
                            std::chrono::microseconds((current_value_ + 1) * 1000ull * 1000ull - 1));
#endif
      stream_.Publish(Number(current_value_));
      ++current_value_;
      std::this_thread::sleep_for(sleep_between_numbers_);
    }
  }

 private:
  std::atomic_int current_value_;
  current::sherlock::Stream<Number> stream_;
  const HTTPRoutesScope http_scope_;
  const std::chrono::microseconds sleep_between_numbers_;
  std::atomic_bool destructing_;
  std::thread thread_;
  current::karl::Claire claire_;
};

}  // namespace karl_unittest

#endif  // KARL_TEST_SERVICE_GENERATOR_H
