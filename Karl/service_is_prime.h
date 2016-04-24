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

#ifndef KARL_SERVICE_IS_PRIME_H
#define KARL_SERVICE_IS_PRIME_H

#include "claire.h"

#include <atomic>

#include "../Blocks/HTTP/api.h"

namespace karl_unittest {

// Lowercase struct name for cleaner JSON format, as it's part of a `Variant<>`.
CURRENT_STRUCT(is_prime) {
  CURRENT_FIELD(test, std::string, "PASS");
  CURRENT_FIELD(requests, uint64_t, 0ull);
  CURRENT_CONSTRUCTOR(is_prime)(uint64_t requests = 0ull) : requests(requests) {}
};

CURRENT_STRUCT(IsPrimeStatus, current::karl::ClaireStatusBase) { CURRENT_FIELD(runtime, Variant<is_prime>); };

class ServiceIsPrime final {
 public:
  explicit ServiceIsPrime(uint16_t port, const current::karl::Locator& karl)
      : counter_(0ull),
        http_scope_(HTTP(port).Register(
            "/is_prime",
            [this](Request r) {
              ++counter_;
              r(IsPrime(current::FromString<int>(r.url.query.get("x", "0"))) ? "YES\n" : "NO\n");
            })),
        claire_(karl, "is_prime", port) {
    const auto status_reporter = [this]() -> is_prime { return is_prime(counter_); };
#ifdef CURRENT_MOCK_TIME
    // In unit test mode, wait for Karl's response and callback, and fail if Karl is not available.
    claire_.Register(status_reporter, true);
#else
    // In example "production" mode just start regular keepalives.
    claire_.Register(status_reporter);
#endif
  }

 private:
  static bool IsPrime(int x) {
    if (x < 2) {
      return false;
    } else {
      for (int i = 2; i * i <= x; ++i) {
        if ((x % i) == 0) {
          return false;
        }
      }
      return true;
    }
  }

  std::atomic<uint64_t> counter_;
  const HTTPRoutesScope http_scope_;
  current::karl::GenericClaire<is_prime, IsPrimeStatus> claire_;
};

}  // namespace karl_unittest

#endif  // KARL_SERVICE_IS_PRIME_H
