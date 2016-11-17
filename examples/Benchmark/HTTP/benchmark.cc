/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../../../current.h"

#include "server.h"

using namespace current;

DEFINE_double(seconds, 2.5, "Run the load test for this many seconds.");
DEFINE_string(url,
              "http://localhost:%d/add",
              "The URL for the load test, default to `http://localhost:${FLAGS_port}`.");
DEFINE_int32(port, PickPortForUnitTest(), "The port to use, if `--url` includes it.");

DEFINE_int32(threads,
             100,
             "The number of threads to run requests from. NOTE: As requests are sent synchronously, the "
             "measurement will be imprecise if (ping) / (time to service the request) is greater than "
             "FLAGS_threads. Thus, this benchmarking tool is not useful when profiling remote servers.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  class Worker {
   public:
    explicit Worker(double seconds) : seconds_(seconds), queries_(0u), thread_(&Worker::Thread, this) {}
    void Join() { thread_.join(); }
    size_t TotalQueries() const { return queries_; }

   private:
    static double NowInSeconds() { return 1e-6 * static_cast<double>(time::Now().count()); }
    void Thread() {
      const double timestamp_end = NowInSeconds() + seconds_;
      while (NowInSeconds() < timestamp_end) {
        const int a = current::random::RandomIntegral(-1000000, +1000000);
        const int b = current::random::RandomIntegral(-1000000, +1000000);
        const auto r = HTTP(GET(strings::Printf(FLAGS_url.c_str(), FLAGS_port) + strings::Printf("?a=%d&b=%d", a, b)));
        CURRENT_ASSERT(r.code == HTTPResponseCode.OK);
        CURRENT_ASSERT(ParseJSON<AddResult>(r.body).sum == a + b);
        ++queries_;
      }
    }

    const double seconds_;
    size_t queries_;
    std::thread thread_;
  };

  std::vector<std::unique_ptr<Worker>> threads(FLAGS_threads);
  for (auto& t : threads) {
    t = std::make_unique<Worker>(FLAGS_seconds);
  }

  for (auto& t : threads) {
    t->Join();
  }

  size_t total_queries = 0u;
  for (auto& t : threads) {
    total_queries += t->TotalQueries();
  }

  std::cout << "QPS: " << std::setw(3) << (total_queries / FLAGS_seconds) << std::endl;
}
