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

// A simple load test for the server.
// Not suitable for large latencies, since it sends requests synchronously from N threads,
// instead of using `epoll()` to maximize client-side throughput.

#include "../../current.h"

#include "schema.h"

using namespace current;

DEFINE_double(seconds, 2.5, "Run the load test for this many seconds.");
DEFINE_string(url,
              "http://localhost:%d",
              "The URL for the load test, default to `http://localhost:${FLAGS_port}`.");
DEFINE_int32(port, 8889, "The port to use, if `--url` includes it.");
DEFINE_int32(threads, 100, "The number of threads to run requests from.");

DEFINE_int32(userid_lengths, 4, "The length of random user IDs to generate.");
DEFINE_int32(nickname_lengths, 6, "The length of random user nicknames to generate.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  class Worker {
   public:
    Worker(const std::string& url, double seconds)
        : url_(url), seconds_(seconds), queries_(0u), thread_(&Worker::Thread, this) {}
    void Join() { thread_.join(); }
    size_t TotalQueries() const { return queries_; }

   private:
    static double NowInSeconds() { return 1e-6 * static_cast<double>(time::Now().count()); }
    void Thread() {
      const Event body = []() {
        UserAdded body;
        body.timestamp = time::Now();
        body.user_id = std::string(FLAGS_userid_lengths, ' ');
        body.nickname = std::string(FLAGS_nickname_lengths, ' ');
        for (auto& c : body.user_id) {
          c = random::RandomIntegral<char>('a', 'z');
        }
        for (auto& c : body.nickname) {
          c = random::RandomIntegral<char>('A', 'Z');
        }
        return Event(body);
      }();
      const double timestamp_begin = NowInSeconds();
      const double timestamp_end = timestamp_begin + seconds_;
      double timestamp_now;
      while ((timestamp_now = NowInSeconds()) < timestamp_end) {
        CURRENT_ASSERT(HTTP(POST(url_, body)).code == HTTPResponseCode.NoContent);
        ++queries_;
      }
    }

    const std::string url_;
    const double seconds_;
    size_t queries_;
    std::thread thread_;
  };

  const std::string url = strings::Printf(FLAGS_url.c_str(), FLAGS_port) + "/publish";

  std::vector<std::unique_ptr<Worker>> threads(FLAGS_threads);
  for (auto& t : threads) {
    t = std::make_unique<Worker>(url, FLAGS_seconds);
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
