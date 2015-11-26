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

// A simple profiler with a `PROFILER_SCOPE("my magical scope")` macro to declare scopes
// and a `PROFILER_HTTP_ROUTE(port, "/route")` macro to define an HTTP endpoint exposing
// a full snapshot of how much time did each thread spend in each scope.
// Scopes are hierarchical, represented in the output as a full call stack tree.

#ifndef CURRENT_PROFILER_H
#define CURRENT_PROFILER_H

#ifndef CURRENT_PROFILER

#define PROFILER_ENABLED false
#define PROFILER_SCOPE(scope)
#define PROFILER_HTTP_ROUTE(port, route)

#else

#ifdef BRICKS_COVERAGE_REPORT_MODE
#error "No `CURRENT_PROFILER` in `BRICKS_COVERAGE_REPORT_MODE` please."
#endif

#include <thread>

#include "../Blocks/HTTP/api.h"
#include "../Bricks/cerealize/json.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/util/singleton.h"

#ifdef BRICKS_MOCK_TIME
#error "No `CURRENT_PROFILER` in `BRICKS_MOCK_TIME` please."
#endif

struct Profiler {
  class StateMaintainer {
   private:
    struct State {
      struct PerThread {
        struct Trie {
          uint64_t ms_entered = 0;               // `Now()` if within it, `0` if currently not there.
          uint64_t ms_total = 0;                 // Total across all the times this scope was entered.
          uint64_t entries = 0;                  // The number of times this scope was entered.
          std::map<const char*, Trie> children;  // Sub-scopes within this scope, if any.
          uint64_t ComputeTotalMilliseconds(uint64_t now) const {
            if (ms_entered) {
              assert(now >= ms_entered);
              return ms_total + (now - ms_entered);
            } else {
              return ms_total;
            }
          }
          void RecursiveReset(uint64_t now) {
            ms_total = 0;
            if (ms_entered) {
              ms_entered = now;
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
          trie.ms_entered = Now();
          trie.entries = 1;
          stack.push(std::make_pair("", &trie));
        }
      };
      struct PerThreadReporting {
        std::string scope;
        uint64_t entries;
        uint64_t ms;
        double ms_per_entry;
        double absolute_best_possible_qps;
        double ratio_of_parent;
        std::vector<PerThreadReporting> subscope;
        double subscope_total_ratio_of_parent;
        bool operator<(const PerThreadReporting& rhs) const {
          return ms > rhs.ms;  // Naturally sort in reverse order of `ms`.
        }
        template <typename A>
        void save(A& ar) const {
          ar(CEREAL_NVP(scope),
             CEREAL_NVP(entries),
             CEREAL_NVP(ms),
             CEREAL_NVP(ms_per_entry),
             CEREAL_NVP(absolute_best_possible_qps),
             CEREAL_NVP(ratio_of_parent),
             CEREAL_NVP(subscope),
             CEREAL_NVP(subscope_total_ratio_of_parent));
        }
      };
      std::unordered_map<std::thread::id, PerThread> per_thread;
      uint64_t total_spent_in_profiling = 0;
      uint64_t spent_in_reporting = 0;
      uint64_t spent_in_mutex_locking = 0;
      void Reset() {
        total_spent_in_profiling = 0;
        spent_in_reporting = 0;
        spent_in_mutex_locking = 0;
        for (auto& e : per_thread) {
          e.second.trie.RecursiveReset(Now());
        }
      }
      static const char* JSONEntryName() { return "milliseconds"; }
      template <typename A>
      void save(A& ar) const {
        const uint64_t now = Now();
        const uint64_t profiling_overhead = total_spent_in_profiling - spent_in_reporting;
        const uint64_t reporting_overhead = spent_in_reporting;
        const uint64_t profiling_mutex_overhead = spent_in_mutex_locking;
        std::vector<PerThreadReporting> thread;
        thread.reserve(per_thread.size());
        for (const auto& per_thread_element : per_thread) {
          std::function<void(
              const PerThread::Trie& input, uint64_t total_ms, PerThreadReporting& output, const char* stack)>
              recursive_fill;
          recursive_fill = [now, &recursive_fill](
              const PerThread::Trie& input, uint64_t total_ms, PerThreadReporting& output, const char* stack) {
            output.scope = stack;
            output.entries = input.entries;
            assert(output.entries);
            output.ms = input.ComputeTotalMilliseconds(now);
            output.ms_per_entry = 1.0 * output.ms / output.entries;
            output.absolute_best_possible_qps = output.ms ? (1000.0 / output.ms_per_entry) : 1e9;
            output.ratio_of_parent = total_ms ? (1.0 * output.ms / total_ms) : 1.0;
            output.subscope.reserve(input.children.size());
            for (const auto& scope : input.children) {
              output.subscope.resize(output.subscope.size() + 1);
              recursive_fill(scope.second, output.ms, output.subscope.back(), scope.first);
            }
            assert(output.subscope.size() == input.children.size());
            uint64_t subscope_total = 0;
            for (const auto& subscope : output.subscope) {
              subscope_total += subscope.ms;
            }
            assert(subscope_total <= output.ms);
            output.subscope_total_ratio_of_parent = output.ms ? (1.0 * subscope_total / output.ms) : 1.0;
            std::sort(output.subscope.begin(), output.subscope.end());
          };
          thread.resize(thread.size() + 1);
          std::ostringstream thread_id_as_string;
          thread_id_as_string << "C++ thread with internal ID " << per_thread_element.first;
          recursive_fill(per_thread_element.second.trie,
                         per_thread_element.second.trie.ComputeTotalMilliseconds(now),
                         thread.back(),
                         thread_id_as_string.str().c_str());
        }
        assert(thread.size() == per_thread.size());
        ar(CEREAL_NVP(thread),
           CEREAL_NVP(profiling_overhead),
           CEREAL_NVP(profiling_mutex_overhead),
           CEREAL_NVP(reporting_overhead));
      }
    };

   public:
    void EnterScope(const char* scope) {
      assert(scope);
      assert(*scope);
      Guarded([scope](uint64_t now, State::PerThread& per_thread) {
        assert(now);
        assert(!per_thread.stack.empty());
        State::PerThread::Trie& node = (*per_thread.stack.top().second).children[scope];
        node.ms_entered = now;
        ++node.entries;
        per_thread.stack.push(std::make_pair(scope, &node));
      });
    }

    void LeaveScope(const char* scope) {
      assert(scope);
      assert(*scope);
      Guarded([scope](uint64_t now, State::PerThread& per_thread) {
        assert(now);
        assert(!per_thread.stack.empty());
        assert(scope == per_thread.stack.top().first);
        State::PerThread::Trie& node = (*per_thread.stack.top().second);
        assert(node.ms_entered <= now);
        node.ms_total += (now - node.ms_entered);
        node.ms_entered = 0;
        per_thread.stack.pop();
        assert(!per_thread.stack.empty());  // Should have at least the root trie node left in the stack.
      });
    }

    void Report(Request request) {
      Guarded([this, &request](uint64_t unused_now, const State::PerThread& unused_per_thread_state) {
        static_cast<void>(unused_now);
        static_cast<void>(unused_per_thread_state);
        if (request.url.query.has("reset")) {
          state_.Reset();
          request("The profiler has been reset.\n");
        } else {
          const uint64_t pre_report = Now();
          request(state_);
          const uint64_t post_report = Now();
          state_.spent_in_reporting += (post_report - pre_report);
        }
      });
    }

   private:
    static uint64_t Now() { return static_cast<uint64_t>(current::time::Now()); }

    template <typename F>
    void Guarded(F&& f) {
      const uint64_t pre_mutex_lock = Now();
      std::lock_guard<std::mutex> lock(mutex_);
      const uint64_t post_mutex_lock = Now();
      f(post_mutex_lock, state_.per_thread[std::this_thread::get_id()]);
      const uint64_t post_guarded_code = Now();
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
      assert(scope);
      assert(*scope);
      current::Singleton<StateMaintainer>().EnterScope(scope_);
    }
    ~ScopedStateMaintainer() { current::Singleton<StateMaintainer>().LeaveScope(scope_); }

   private:
    ScopedStateMaintainer() = delete;
    const char* const scope_;
  };

  static void HTTPRoute(Request request) { current::Singleton<StateMaintainer>().Report(std::move(request)); }
};

#define PROFILER_ENABLED true

// Preprocessor token pasting occurs before recursive macro expansion, hence three-stage magic.
#define PROFILER_SCOPE_CONCATENATE_HELPER_IMPL(a, b) a##b
#define PROFILER_SCOPE_CONCATENATE_HELPER(a, b) PROFILER_SCOPE_CONCATENATE_HELPER_IMPL(a, b)
#define PROFILER_SCOPE(scope) \
  Profiler::ScopedStateMaintainer PROFILER_SCOPE_CONCATENATE_HELPER(profiler_scope_, __LINE__)(scope)

#define PROFILER_HTTP_ROUTE(port, route)                                      \
  do {                                                                        \
    std::cerr << "Profiler: http://localhost:" << port << route << std::endl; \
    HTTP(port).Register(route, Profiler::HTTPRoute);                          \
  } while (false)

#endif

#endif  // CURRENT_PROFILER_H
