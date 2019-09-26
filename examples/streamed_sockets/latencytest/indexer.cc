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

#include "workers/indexer.h"
#include "blob.h"
#include "next_ripcurrent.h"
#include "workers/receiver.h"
#include "workers/saver.h"
#include "workers/sender.h"

#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/time/chrono.h"

DEFINE_uint16(listen_port, 8001, "The local port to listen on.");
DEFINE_string(host1, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port1, 8002, "The destination port to send data to.");
DEFINE_string(host2, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port2, 8003, "The destination port to send data to.");
DEFINE_double(buffer_mb, 32.0, "The size of the circular buffer to use, in megabytes.");
DEFINE_uint64(max_index_block, 512, "Index max. this many entries per state mutex lock, throttling.");
DEFINE_string(dirname, ".current", "The dir name for the stored data files.");
DEFINE_string(filebase, "idx.", "The filename prefix for the stored data files.");
DEFINE_uint64(blobs_per_file,
              (1 << 28) / sizeof(current::examples::streamed_sockets::Blob),
              "The number of blobs per file saved, defaults to 256MB files.");
DEFINE_uint32(max_total_files, 4u, "The maximum number of data files to keep, defaults to four files, for 1GB total.");
DEFINE_bool(wipe_files_at_startup, true, "Unset to not wipe the files from the previous run.");
DEFINE_bool(skip_fwrite, false, "Set to not `fwrite()` into the files; for network perftesting only.");

namespace current::examples::streamed_sockets {

struct One;
struct Two;

struct State {
  volatile size_t read = 0u;
  volatile size_t indexed = 0u;
  volatile size_t saved = 0u;
  volatile size_t sent1 = 0u;
  volatile size_t sent2 = 0u;
};

template <>
struct OutputOf<State, ReceivingWorker> {
  static void Set(State& state, size_t value) { state.read = value; }
};

template <>
struct InputOf<State, IndexingWorker> {
  static size_t Get(const State& state) { return state.read; }
};

template <>
struct InputOf<State, SavingWorker> {
  static size_t Get(const State& state) { return state.indexed; }
};

template <>
struct InputOf<State, SendingWorker<One>> {
  static size_t Get(const State& state) { return state.indexed; }
};

template <>
struct InputOf<State, SendingWorker<Two>> {
  static size_t Get(const State& state) { return state.indexed; }
};

template <>
struct OutputOf<State, IndexingWorker> {
  static void Set(State& state, size_t value) { state.indexed = value; }
};

template <>
struct OutputOf<State, SavingWorker> {
  static void Set(State& state, size_t value) { state.saved = value; }
};

template <>
struct OutputOf<State, SendingWorker<One>> {
  static void Set(State& state, size_t value) { state.sent1 = value; }
};

template <>
struct OutputOf<State, SendingWorker<Two>> {
  static void Set(State& state, size_t value) { state.sent2 = value; }
};

template <>
struct SinkOf<State> {
  static size_t Get(const State& state) { return std::min(state.saved, std::min(state.sent1, state.sent2)); }
};

inline void RunIndexer() {
  const size_t min_n = std::max(static_cast<size_t>(8u), static_cast<size_t>(1e6 * FLAGS_buffer_mb) / sizeof(Blob));
  const size_t actual_n = [min_n]() {
    size_t min_power_of_to_ge_min_n = 1u;
    while (min_power_of_to_ge_min_n < min_n) {
      min_power_of_to_ge_min_n *= 2u;
    }
    return min_power_of_to_ge_min_n;
  }();

  std::vector<Blob> buffer(actual_n);

  current::WaitableAtomic<State> mutable_state;

  std::thread t_source = SpawnThreadSource<ReceivingWorker>(buffer, mutable_state, FLAGS_listen_port);
  std::thread t_indexing = SpawnThreadWorker<IndexingWorker>(buffer, mutable_state, FLAGS_max_index_block);
  std::thread t_save = SpawnThreadWorker<SavingWorker>(buffer,
                                                       mutable_state,
                                                       FLAGS_dirname,
                                                       FLAGS_filebase,
                                                       FLAGS_blobs_per_file,
                                                       FLAGS_max_total_files,
                                                       FLAGS_wipe_files_at_startup,
                                                       FLAGS_skip_fwrite);
  std::thread t_send1 = SpawnThreadWorker<SendingWorker<One>>(buffer, mutable_state, FLAGS_host1, FLAGS_port1);
  std::thread t_send2 = SpawnThreadWorker<SendingWorker<Two>>(buffer, mutable_state, FLAGS_host2, FLAGS_port2);

  t_source.join();
  t_indexing.join();
  t_save.join();
  t_send1.join();
  t_send2.join();
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

  current::examples::streamed_sockets::RunIndexer();
}
