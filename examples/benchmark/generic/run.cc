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

#include "../../../current.h"

#include "scenario_golden_1k_qps.h"
#include "scenario_json.h"
#include "scenario_simple_http.h"
#include "scenario_storage.h"
#include "scenario_nginx_client.h"
#include "scenario_replication.h"

using namespace current;

DEFINE_string(scenario, "", "Benchmarking scenario to run. Leave empty for synopsis.");

DEFINE_double(seconds, 2.5, "Run the load test for this many seconds.");

DEFINE_int32(threads,
             24,
             "The number of threads to run requests from. NOTE: As individual queries are run synchronously, "
             "the measurement may be imprecise when run against a high-latency network,"
             "as more time would be spent waiting than running. Thus, this tool is only good for local tests.");

static double NowInSeconds() {
  // Don't use `current::time::Now()`, as it's under a mutex, and guaranteed to increase by at least 1 per call.
  return 1e-6 *
         std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
             .count();
}

template <typename SCENARIO>
void Run(const SCENARIO& scenario) {
  struct Thread {
    Thread(const SCENARIO& scenario, double total_seconds)
        : scenario_(scenario),
          wall_time_second_end_(NowInSeconds() + total_seconds),
          queries_completed_within_desired_timeframe_(0u),
          thread_(&Thread::ThreadFunction, this) {}

    void Join() { thread_.join(); }

    const SCENARIO& scenario_;
    const double wall_time_second_end_;
    size_t queries_completed_within_desired_timeframe_;
    std::thread thread_;

    // The thread function runs the queries continuously. It only counts the queries
    // completed within the originally desired number of seconds in the final number.
    void ThreadFunction() {
      while (true) {
        scenario_->RunOneQuery();

        if (NowInSeconds() >= wall_time_second_end_) {
          break;
        }

        ++queries_completed_within_desired_timeframe_;
      }
    }
  };

  std::vector<std::unique_ptr<Thread>> threads(FLAGS_threads);
  for (auto& t : threads) {
    t = std::make_unique<Thread>(scenario, FLAGS_seconds);
  }

  for (auto& t : threads) {
    t->Join();
  }

  size_t total_queries = 0u;
  for (auto& t : threads) {
    total_queries += t->queries_completed_within_desired_timeframe_;
  }

  const double qps = total_queries / FLAGS_seconds;
  std::cout << std::setw(3) << qps << " QPS." << std::endl;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  const auto& registerer = Singleton<ScenariosRegisterer>();
  if (FLAGS_scenario.empty()) {
    registerer.Synopsis();
    return 1;
  } else {
    try {
      Run(registerer.map.at(FLAGS_scenario).second());
      return 0;
    } catch (const std::out_of_range&) {
      std::cout << "Scenario `" << FLAGS_scenario << "` is not defined." << std::endl;
      return -1;
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  }
}
