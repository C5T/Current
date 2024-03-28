#include <mutex>

#include "../../blocks/http/api.h"
#include "../../bricks/dflags/dflags.h"
#include "../../bricks/time/chrono.h"

DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_uint32(m, 10, "The number of concurrent calls to make.");

// Here is a possible reference output when this binary is run after the server is started:
//
// response 'ok from thread 0' to thread '0' in 0.1 seconds
// response 'ok from thread 1' to thread '3' in 0.1 seconds
// response 'ok from thread 2' to thread '2' in 0.1 seconds
// response 'ok from thread 2' to thread '4' in 0.2 seconds
// response 'ok from thread 0' to thread '5' in 0.2 seconds
// response 'ok from thread 1' to thread '6' in 0.2 seconds
// response 'ok from thread 2' to thread '1' in 0.3 seconds
// response 'ok from thread 1' to thread '9' in 0.3 seconds
// response 'ok from thread 0' to thread '7' in 0.3 seconds
// response 'ok from thread 2' to thread '8' in 0.4 seconds
//
// Note that with the default "three threads, 0.1 seconds delay" server setup, the overall run takes 0.4 seconds,
// with responses coming in in batches of three, followed by the lone tenth response.

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  std::mutex mutex;
  auto const start_us = current::time::Now();

  std::vector<std::thread> callers(FLAGS_m);
  for (size_t i = 0u; i < callers.size(); ++i) {
    callers[i] = std::thread([i, start_us, &mutex]() {
      auto const r = HTTP(GET("http://localhost:" + current::ToString(FLAGS_port))).body;
      std::lock_guard lock(mutex);
      std::cout << current::strings::Printf("response '%s' to thread '%d' in %.1lf seconds",
                                            current::strings::Trim(r).c_str(),
                                            int(i),
                                            1e-6 * (current::time::Now() - start_us).count())
                << std::endl;
    });
  }

  for (auto& t : callers) {
    t.join();
  }
}
