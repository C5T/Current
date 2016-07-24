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

#ifndef BENCHMARK_SCENARIO_GOLDEN_1K_QPS_H
#define BENCHMARK_SCENARIO_GOLDEN_1K_QPS_H

#include "../../../port.h"

#include <chrono>
#include <thread>
#include <mutex>

#include "benchmark.h"

#include "../../../Bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_double(golden_qps, 1000, "Golden QPS for the `golden_1kqps` scenario to use.");
#else
DECLARE_double(golden_qps);
#endif

// clang-format off
SCENARIO(golden_1kqps, "Golden 1000 QPS.") {
  // Don't use `current::time::Now()`, as it's:
  // a) under an unnecessary mutex, and
  // b) guaranteed to increase by at least 1 per call.
  static std::chrono::microseconds LocalNow() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch());
  }

  void RunOneQuery() override {
    // Carefully make sure the next query completes execution in exactly 1000us after the previous one.
    // Configurable via `--golden_qps`.
    const auto sleep_us = std::chrono::microseconds(static_cast<int64_t>(1e6 / FLAGS_golden_qps));

    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    std::chrono::microseconds now = LocalNow();

    static std::chrono::microseconds next_wall_time_us = LocalNow();

    if (next_wall_time_us > now) {
      std::this_thread::sleep_for(next_wall_time_us - now);
    }

    next_wall_time_us += sleep_us;
  }
};
// clang-format on

REGISTER_SCENARIO(golden_1kqps);

#endif  // BENCHMARK_SCENARIO_GOLDEN_1K_QPS_H
