/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "workers/processor.h"
#include "blob.h"
#include "next_ripcurrent.h"
#include "workers/receiver.h"

#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"

DEFINE_uint16(listen_port, 8004, "The local port to listen on.");
DEFINE_string(host, "127.0.0.1", "The destination address to send the confirmations to.");
DEFINE_uint16(port, 8005, "The destination port to send the confirmations to.");
DEFINE_double(buffer_mb, 512.0, "The size of the circular buffer to use, in megabytes.");

namespace current::examples::streamed_sockets {

struct State {
  volatile size_t read = 0u;
  volatile size_t processed = 0u;
};

template <>
struct OutputOf<State, ReceivingWorker> {
  static void Set(State& state, size_t value) { state.read = value; }
};

template <>
struct InputOf<State, ProcessingWorker> {
  static size_t Get(const State& state) { return state.read; }
};

template <>
struct OutputOf<State, ProcessingWorker> {
  static void Set(State& state, size_t value) { state.processed = value; }
};

template <>
struct SinkOf<State> {
  static size_t Get(const State& state) { return state.processed; }
};

inline void RunProcess() {
  const size_t min_n = std::max(static_cast<size_t>(8u), static_cast<size_t>(1e6 * FLAGS_buffer_mb) / sizeof(Blob));
  const size_t actual_n = [min_n]() {
    size_t min_power_of_two_greater_than_or_equals_to_min_n = 1u;
    while (min_power_of_two_greater_than_or_equals_to_min_n < min_n) {
      min_power_of_two_greater_than_or_equals_to_min_n *= 2u;
    }
    return min_power_of_two_greater_than_or_equals_to_min_n;
  }();

  std::vector<Blob> buffer(actual_n);

  current::WaitableAtomic<State> mutable_state;

  std::thread t_source = SpawnThreadSource<ReceivingWorker>(buffer, mutable_state, FLAGS_listen_port);
  std::thread t_process = SpawnThreadWorker<ProcessingWorker>(buffer, mutable_state, FLAGS_host, FLAGS_port);

  t_source.join();
  t_process.join();
}

}  // namespace current::examples::streamed_sockets

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

#ifndef NDEBUG
  {
    using namespace current::vt100;
    std::cout << yellow << bold << "warning" << reset << ": unoptimized build";
#if defined(CURRENT_POSIX) || defined(CURRENT_APPLE)
    std::cout << ", run " << cyan << "NDEBUG=1 make clean all";
#endif
    std::cout << std::endl;
  }
#endif

  current::examples::streamed_sockets::RunProcess();
}
