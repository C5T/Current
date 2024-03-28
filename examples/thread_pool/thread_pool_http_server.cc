#include <memory>
#include <mutex>
#include <queue>

#include "../../blocks/http/api.h"
#include "../../bricks/dflags/dflags.h"
#include "../../bricks/sync/waitable_atomic.h"

DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_uint32(n, 3, "The number of threads to use in the thread pool.");
DEFINE_double(delay_s, 0.1, "The number of seconds to wait before responding via HTTP.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  try {
    auto& http = HTTP(current::net::AcquireLocalPort(FLAGS_port));

    struct SharedState final {
      bool die = false;
      std::queue<Request> reqs;
    };
    current::WaitableAtomic<SharedState> safe_state;

    auto scope = http.Register("/", [&safe_state](Request r) {
      safe_state.MutableUse([](SharedState& state, Request r) { state.reqs.push(std::move(r)); }, std::move(r));
    });

    scope += http.Register("/kill", [&safe_state](Request r) {
      r("dying\n");
      std::cout << "dying by request" << std::endl;
      safe_state.MutableScopedAccessor()->die = true;
    });

    std::vector<std::thread> threads(FLAGS_n);
    std::mutex cout_mutex;
    for (size_t i = 0; i < threads.size(); ++i) {
      threads[i] = std::thread([&safe_state, &cout_mutex, i]() {
        while (true) {
          struct OptionalRequestOrDie final {
            bool die = false;
            std::unique_ptr<Request> r;

            // The default constructor is invoked if the wait timed out.
            OptionalRequestOrDie() {}

            // The explicit constructor to signals it is time to die.
            struct TimeToDie final {};
            OptionalRequestOrDie(TimeToDie) : die(true) {}

            // The happy path constructor, capture the request to respond to next.
            OptionalRequestOrDie(Request r) : r(std::make_unique<Request>(std::move(r))) {}
          };
          auto req = safe_state.WaitFor(
              [](SharedState const& state) { return state.die || !state.reqs.empty(); },
              [](SharedState& state) {
                if (state.die) {
                  return OptionalRequestOrDie(OptionalRequestOrDie::TimeToDie());
                } else {
                  auto req = OptionalRequestOrDie(std::move(state.reqs.front()));
                  state.reqs.pop();
                  return req;
                }
              },
              std::chrono::seconds(1));
          if (req.die) {
            // Time to die. All the threads will see it and terminate.
            break;
          } else if (req.r) {
            // NOTE(dkorolev): This could be done with `.WaitFor()` to not wait "the remainder of the second" when dying.
            std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(1'000 * FLAGS_delay_s)));
            (*req.r)("ok from thread " + current::ToString(i) + '\n');
          } else {
            std::lock_guard cout_lock(cout_mutex);
            std::cout << "thread " + current::ToString(i) + " is still waiting" << std::endl;
          }
        }
      });
    }

    std::cout << "listening with " << FLAGS_n << " threads on port " << FLAGS_port << std::endl;

    for (auto& t : threads) {
      t.join();
    }
  }
  catch (current::net::SocketBindException const&) {
    std::cout << "the local port " << FLAGS_port << " is already taken" << std::endl;
  }
}
