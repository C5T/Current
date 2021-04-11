#include <iostream>
#include <thread>

#include "progress.h"
#include "vt100.h"

#include "../../bricks/util/random.h"

int main() {
  current::MultilineProgress multiline_progress;
  std::vector<std::thread> threads;
  int index = 0u;
  do {
    threads.emplace_back([&multiline_progress](int index) {
        current::ProgressLine line = multiline_progress();
        const double k = current::random::RandomDouble(0.01, 0.04);
        const double b = current::random::RandomDouble(0.0, 6.28);
        const double a = current::random::RandomDouble(20, 45);
        const auto t0 = current::time::Now();
        while (true) {
          line << "Line" << index << ": " << std::string(static_cast<size_t>(
            0.5 * (std::sin((current::time::Now() - t0).count() * 1e-3 * k + b) + 1.0) * a), '*');
          std::this_thread::sleep_for(std::chrono::milliseconds(current::random::RandomInt(1, 25)));
        }
    }, ++index);
    std::this_thread::sleep_for(std::chrono::milliseconds(current::random::RandomInt(500, 2500 * index)));
  } while (true);
}
