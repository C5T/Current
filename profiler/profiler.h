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

// A simple profiler with a `CURRENT_PROFILER_SCOPE("my magical scope")` macro to declare scopes and
// a `CURRENT_PROFILER_HTTP_ROUTE(http_scopes_variable, port, "/route")` macro to define an HTTP endpoint
// exposing a full snapshot of how much time did each thread spend in each scope.
// Scopes are hierarchical, represented in the output as a full call stack tree.

#ifndef CURRENT_PROFILER_H
#define CURRENT_PROFILER_H

#ifndef CURRENT_PROFILER

#define CURRENT_PROFILER_SCOPE(scope)
#define CURRENT_PROFILER_HTTP_ROUTE(scope, port, route)

#else

#ifdef CURRENT_COVERAGE_REPORT_MODE
#error "No `CURRENT_PROFILER` in `CURRENT_COVERAGE_REPORT_MODE` please."
#endif

#include <thread>

#include "../blocks/http/api.h"
#include "../bricks/time/chrono.h"
#include "../bricks/util/singleton.h"

#ifdef CURRENT_MOCK_TIME
#error "No `CURRENT_PROFILER` in `CURRENT_MOCK_TIME` please."
#endif

namespace current {
namespace profiler {

CURRENT_STRUCT(PerThreadReporting) {
  CURRENT_FIELD(scope, std::string);
  CURRENT_FIELD(entries, uint64_t);
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(us_per_entry, double);
  CURRENT_FIELD(absolute_best_possible_qps, double);
  CURRENT_FIELD(ratio_of_parent, double);
  CURRENT_FIELD(subscope, std::vector<PerThreadReporting>);
  CURRENT_FIELD(subscope_total_ratio_of_parent, double);
  bool operator<(const PerThreadReporting& rhs) const {
    return us > rhs.us;  // Naturally sort in reverse order of `us`.
  }
};

CURRENT_STRUCT(ProfilingReport) {
  CURRENT_FIELD(thread, std::vector<PerThreadReporting>);
  CURRENT_FIELD(profiling_overhead, std::chrono::microseconds);
  CURRENT_FIELD(profiling_mutex_overhead, std::chrono::microseconds);
  CURRENT_FIELD(reporting_overhead, std::chrono::microseconds);
};

struct Profiler {
  class StateMaintainer {
   private:
    struct State {
      struct PerThread {
        struct Trie {
          // `Now()` if within it, `0` if currently not there.
          std::chrono::microseconds us_entered = std::chrono::microseconds(0);
          // Total across all the times this scope was entered.
          std::chrono::microseconds us_total = std::chrono::microseconds(0);
          // The number of times this scope was entered.
          uint64_t entries = 0;
          // Sub-scopes within this scope, if any.
          std::map<const char*, Trie> children;

          std::chrono::microseconds ComputeTotalMicroseconds(std::chrono::microseconds now) const {
            if (us_entered.count()) {
              CURRENT_ASSERT(now >= us_entered);
              return std::chrono::microseconds(us_total.count() + (now - us_entered).count());
            } else {
              return us_total;
            }
          }
          void RecursiveReset(std::chrono::microseconds now) {
            us_total = std::chrono::microseconds(0);
            if (us_entered.count()) {
              us_entered = now;
            }
            entries = 1;
            for (auto& e : children) {
              e.second.RecursiveReset(now);
            }
          }
        };
        Trie trie;
        std::stack<std::pair<const char*, Trie*> > stack;
        PerThread() {
          trie.us_entered = current::time::Now();
          trie.entries = 1;
          stack.push(std::make_pair("", &trie));
        }
      };
      std::unordered_map<std::thread::id, PerThread> per_thread;
      std::chrono::microseconds total_spent_in_profiling = std::chrono::microseconds(0);
      std::chrono::microseconds spent_in_reporting = std::chrono::microseconds(0);
      std::chrono::microseconds spent_in_mutex_locking = std::chrono::microseconds(0);
      void Reset() {
        total_spent_in_profiling = std::chrono::microseconds(0);
        spent_in_reporting = std::chrono::microseconds(0);
        spent_in_mutex_locking = std::chrono::microseconds(0);
        for (auto& e : per_thread) {
          e.second.trie.RecursiveReset(current::time::Now());
        }
      }
      ProfilingReport GenerateReport() const {
        const std::chrono::microseconds now = current::time::Now();
        ProfilingReport report;
        report.profiling_overhead = total_spent_in_profiling - spent_in_reporting;
        report.reporting_overhead = spent_in_reporting;
        report.profiling_mutex_overhead = spent_in_mutex_locking;
        std::vector<PerThreadReporting> thread;
        thread.reserve(per_thread.size());
        for (const auto& per_thread_element : per_thread) {
          std::function<void(const PerThread::Trie& input,
                             std::chrono::microseconds total_us,
                             PerThreadReporting& output,
                             const char* stack)>
              recursive_fill;
          recursive_fill = [now, &recursive_fill](const PerThread::Trie& input,
                                                  std::chrono::microseconds total_us,
                                                  PerThreadReporting& output,
                                                  const char* stack) {
            output.scope = stack;
            output.entries = input.entries;
            CURRENT_ASSERT(output.entries);
            output.us = input.ComputeTotalMicroseconds(now);
            output.us_per_entry = 1.0 * output.us.count() / output.entries;
            output.absolute_best_possible_qps = output.us.count() ? (1e6 / output.us_per_entry) : 1e6;
            output.ratio_of_parent = total_us.count() ? (1.0 * output.us.count() / total_us.count()) : 1.0;
            output.subscope.reserve(input.children.size());
            for (const auto& scope : input.children) {
              output.subscope.resize(output.subscope.size() + 1);
              recursive_fill(scope.second, output.us, output.subscope.back(), scope.first);
            }
            CURRENT_ASSERT(output.subscope.size() == input.children.size());
            std::chrono::microseconds subscope_total = std::chrono::microseconds(0);
            for (const auto& subscope : output.subscope) {
              subscope_total += subscope.us;
            }
            CURRENT_ASSERT(subscope_total <= output.us);
            output.subscope_total_ratio_of_parent =
                output.us.count() ? (1.0 * subscope_total.count() / output.us.count()) : 1.0;
            std::sort(output.subscope.begin(), output.subscope.end());
          };
          thread.resize(thread.size() + 1);
          std::ostringstream thread_id_as_string;
          thread_id_as_string << "C++ thread with internal ID " << per_thread_element.first;
          recursive_fill(per_thread_element.second.trie,
                         per_thread_element.second.trie.ComputeTotalMicroseconds(now),
                         thread.back(),
                         thread_id_as_string.str().c_str());
        }
        CURRENT_ASSERT(thread.size() == per_thread.size());
        report.thread = thread;
        return report;
      }
    };

   public:
    void EnterScope(const char* scope) {
      CURRENT_ASSERT(scope);
      CURRENT_ASSERT(*scope);
      Guarded([scope](std::chrono::microseconds now, State::PerThread& per_thread) {
        CURRENT_ASSERT(now.count());
        CURRENT_ASSERT(!per_thread.stack.empty());
        State::PerThread::Trie& node = (*per_thread.stack.top().second).children[scope];
        node.us_entered = now;
        ++node.entries;
        per_thread.stack.push(std::make_pair(scope, &node));
      });
    }

    void LeaveScope(const char* scope) {
      CURRENT_ASSERT(scope);
      CURRENT_ASSERT(*scope);
      Guarded([scope](std::chrono::microseconds now, State::PerThread& per_thread) {
        CURRENT_ASSERT(now.count());
        CURRENT_ASSERT(!per_thread.stack.empty());
        CURRENT_ASSERT(scope == per_thread.stack.top().first);
        State::PerThread::Trie& node = (*per_thread.stack.top().second);
        CURRENT_ASSERT(node.us_entered <= now);
        node.us_total += (now - node.us_entered);
        node.us_entered = std::chrono::microseconds(0);
        per_thread.stack.pop();
        CURRENT_ASSERT(!per_thread.stack.empty());  // Should have at least the root trie node left in the stack.
      });
    }

    void Report(Request request) {
      Guarded([this, &request](std::chrono::microseconds unused_now, const State::PerThread& unused_per_thread_state) {
        static_cast<void>(unused_now);
        static_cast<void>(unused_per_thread_state);
        if (request.url.query.has("reset")) {
          state_.Reset();
          request("The profiler has been reset.\n");
        } else {
          const std::chrono::microseconds pre_report = current::time::Now();
          request(state_.GenerateReport());
          const std::chrono::microseconds post_report = current::time::Now();
          state_.spent_in_reporting += (post_report - pre_report);
        }
      });
    }

   private:
    template <typename F>
    void Guarded(F&& f) {
      const std::chrono::microseconds pre_mutex_lock = current::time::Now();
      std::lock_guard<std::mutex> lock(mutex_);
      const std::chrono::microseconds post_mutex_lock = current::time::Now();
      f(post_mutex_lock, state_.per_thread[std::this_thread::get_id()]);
      const std::chrono::microseconds post_guarded_code = current::time::Now();
      state_.total_spent_in_profiling += (post_guarded_code - pre_mutex_lock);
      state_.spent_in_mutex_locking += (post_mutex_lock - pre_mutex_lock);
    }

    struct Scope {
      Scope(const char*) {}
      const char* scope;
    };

    std::mutex mutex_;
    State state_;
  };

  class ScopedStateMaintainer {
   public:
    explicit ScopedStateMaintainer(const char* scope) : scope_(scope) {
      CURRENT_ASSERT(scope);
      CURRENT_ASSERT(*scope);
      current::Singleton<StateMaintainer>().EnterScope(scope_);
    }
    ~ScopedStateMaintainer() { current::Singleton<StateMaintainer>().LeaveScope(scope_); }

   private:
    ScopedStateMaintainer() = delete;
    const char* const scope_;
  };

  static void HTTPRoute(Request request) { current::Singleton<StateMaintainer>().Report(std::move(request)); }
};

// Preprocessor token pasting occurs before recursive macro expansion, hence three-stage magic.
#define CURRENT_PROFILER_SCOPE_CONCATENATE_HELPER_IMPL(a, b) a##b
#define PROFILER_SCOPE_CONCATENATE_HELPER(a, b) CURRENT_PROFILER_SCOPE_CONCATENATE_HELPER_IMPL(a, b)
#define CURRENT_PROFILER_SCOPE(scope)                                                                     \
  ::current::profiler::Profiler::ScopedStateMaintainer PROFILER_SCOPE_CONCATENATE_HELPER(profiler_scope_, \
                                                                                         __LINE__)(scope)

#define CURRENT_PROFILER_HTTP_ROUTE(scope, port, route)                            \
  do {                                                                             \
    std::cerr << "Profiler: http://localhost:" << port << route << std::endl;      \
    scope += HTTP(port).Register(route, ::current::profiler::Profiler::HTTPRoute); \
  } while (false)

}  // namespace profiler
}  // namespace current

#endif

#endif  // CURRENT_PROFILER_H
