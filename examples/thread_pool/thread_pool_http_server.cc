#include <memory>
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
    for (size_t i = 0; i < threads.size(); ++i) {
      threads[i] = std::thread([&safe_state, i]() {
        while (true) {
          bool die = false;
          safe_state.Wait([&die](SharedState const& state) {
            if (state.die) {
              die = true;
              return true;
            } else {
              return !state.reqs.empty();
            }
          });
          if (die) {
            break;
          }
          // TODO(dkorolev): Tweak two to `WaitableAtomic`: offer `Wait(wait_cb, post_wait_cb)` to only lock it once.
          struct OptionalRequest final {
            Request r;
            explicit OptionalRequest(Request r) : r(std::move(r)) {}
          };
          auto req = safe_state.MutableUse([](SharedState& state) -> std::unique_ptr<OptionalRequest> {
            // NOTE(dkorolev): And now this extra check is super ugly, but necessary. Tweak Two would make it go away!
            if (!state.reqs.empty()) {
              auto req = std::make_unique<OptionalRequest>(std::move(state.reqs.front()));
              state.reqs.pop();
              return req;
            } else {
              return nullptr;
            }
          });
          if (req) {
            std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(1'000 * FLAGS_delay_s)));
            req->r("ok from thread " + current::ToString(i) + '\n');
          }
        }
      });
    }

    std::cout << "listening with " << FLAGS_n << " threads on port " << FLAGS_port << std::endl;

    for (auto& t : threads) {
      t.join();
    }
  } catch (current::net::SocketBindException const&) {
    std::cout << "the local port " << FLAGS_port << " is already taken" << std::endl;
  }
}
