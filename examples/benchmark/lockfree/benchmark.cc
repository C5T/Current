/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/time/chrono.h"

DEFINE_uint32(threads, 24, "The number of threads to iterate from.");
DEFINE_uint32(iterations, 1000, "Call the Now() functions from each thread this many times.");

namespace current_time_with_mutex {
struct EpochClockGuaranteeingMonotonicity {
  mutable uint64_t monotonic_now_us = 0ull;
  mutable std::mutex mutex;

  inline std::chrono::microseconds Now() const {
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t now_us =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    monotonic_now_us = std::max(monotonic_now_us + 1, now_us);
    return std::chrono::microseconds(monotonic_now_us);
  }
};

inline std::chrono::microseconds Now() { return current::Singleton<EpochClockGuaranteeingMonotonicity>().Now(); }
}  // namespace current_time_with_mutex

template <typename QUERY>
std::chrono::microseconds Run() {
  class Thread {
   public:
    explicit Thread(uint64_t iterations) : iterations_(iterations), thread_(&Thread::Iterate, this) {}
    void Join() { thread_.join(); }
    void Iterate() {
      QUERY query;
      for (uint32_t i = 0; i < iterations_; ++i) {
        query();
      }
    }

   private:
    const uint64_t iterations_;
    std::thread thread_;
  };

  std::vector<std::unique_ptr<Thread>> threads(FLAGS_threads);
  const auto start =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
  for (auto& thread : threads) {
    thread = std::make_unique<Thread>(FLAGS_iterations);
  }
  for (auto& thread : threads) {
    thread->Join();
  }
  const auto end =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
  return (end - start) / FLAGS_threads;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  struct NowWithMutex {
    inline std::chrono::microseconds operator()() { return current_time_with_mutex::Now(); }
  };

  struct NowWithAtomic {
    inline std::chrono::microseconds operator()() { return current::time::Now(); }
  };

  std::cout << "Now() with atomic:\t" << Run<NowWithAtomic>().count() << std::endl;
  std::cout << "Now() with mutex:\t" << Run<NowWithMutex>().count() << std::endl;
  return 0;
}
