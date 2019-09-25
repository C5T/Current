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

#include <deque>

#include "blob.h"

#include "../../../blocks/xterm/progress.h"
#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/strings/printf.h"
#include "../../../bricks/sync/waitable_atomic.h"
#include "../../../bricks/time/chrono.h"
#include "../../../bricks/util/random.h"

DEFINE_string(host, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port, 8001, "The destination port to send data to.");
DEFINE_double(send_buffer_mb, 5.0, "Send buffer size.");
DEFINE_uint16(listen_port, 8005, "The port to listen for the confirmations of own requests, to measure latency.");
DEFINE_double(latency_output_frequency, 0.1, "The frequency of latency outputs, in seconds.");
DEFINE_double(latency_measurement_time_window, 5.0, "The width of the latency measurement time window, in seconds.");
DEFINE_bool(evensodds, false, "Set to measure latencies of first and last blob per block separately.");

struct LatencyTracker {
  const std::chrono::microseconds t_time_window_width;
  size_t n = 0u;
  int64_t sum = 0;
  std::deque<std::pair<std::chrono::microseconds, int64_t>> q;
  LatencyTracker() : t_time_window_width(static_cast<int64_t>(1e6 * FLAGS_latency_measurement_time_window)) {}
  void Add(std::chrono::microseconds ts, int64_t latency) {
    ++n;
    q.emplace_back(ts, latency);
    sum += latency;
    const std::chrono::microseconds t_cutoff = ts - t_time_window_width;
    while (q.front().first < t_cutoff) {
      --n;
      sum -= q.front().second;
      q.pop_front();
    }
  }
  std::string State() const {
    if (!n) {
      return "N/A";
    } else {
      std::vector<int64_t> v;
      v.reserve(n);
      for (const auto& e : q) {
        v.push_back(e.second);
      }
      std::ostringstream os;
      int64_t* b = &v[0];
      int64_t* e = &v[0] + n;
      int64_t p;

      os << current::strings::Printf("%.1lfms", 1e-3 * (sum / n));

      p = n / 100;
      std::nth_element(b, b + p, e);
      os << current::strings::Printf(" + %.1lfms", 1e-3 * b[p]);

      p = n / 10;
      std::nth_element(b, b + p, e);
      os << current::strings::Printf(" / %.1lfms", 1e-3 * b[p]);

      p = n / 2;
      std::nth_element(b, b + p, e);
      os << current::strings::Printf(" / %.1lfms", 1e-3 * b[p]);

      p = n * 9 / 10;
      std::nth_element(b, b + p, e);
      os << current::strings::Printf(" / %.1lfms", 1e-3 * b[p]);

      p = n * 99 / 100;
      std::nth_element(b, b + p, e);
      os << current::strings::Printf(" / %.1lfms", 1e-3 * b[p]);

      os << " + N=" << n;

      return os.str();
    }
  }
};

using deque_t = std::deque<std::pair<uint64_t, int64_t>>;  // { request sequence index, timestamp in us }.
using deques_t = std::pair<deque_t, deque_t>;              // sent, received
inline void RelaxQueues(deques_t& dqs, current::ProgressLine& progress) {
  const std::chrono::microseconds t_output_frequency =
      std::chrono::microseconds(static_cast<int64_t>(1e6 * FLAGS_latency_output_frequency));
  static std::chrono::microseconds t_next_output = current::time::Now() + t_output_frequency;

  static LatencyTracker all;
  static LatencyTracker evens;
  static LatencyTracker odds;

  while (!dqs.first.empty() && !dqs.second.empty() && dqs.first.front().first == dqs.second.front().first) {
    const uint64_t req_id = dqs.first.front().first;
    const int64_t ts_sent = dqs.first.front().second;
    const int64_t ts_received = dqs.second.front().second;
    // std::cerr << req_id << '\t' << ts_received - ts_sent << '\n';
    dqs.first.pop_front();
    dqs.second.pop_front();

    const std::chrono::microseconds t_now = current::time::Now();
    all.Add(t_now, ts_received - ts_sent);
    if (req_id & 1) {
      odds.Add(t_now, ts_received - ts_sent);
    } else {
      evens.Add(t_now, ts_received - ts_sent);
    }
    if (t_now >= t_next_output) {
      if (!FLAGS_evensodds) {
        progress << odds.State();  // The `odds` ones are the slowest ones. -- D.K.
      } else {
        progress << all.State() << " | " << evens.State() << " | " << odds.State();
      }
      t_next_output = t_now + t_output_frequency;
    }
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  using namespace current::vt100;
  using current::examples::streamed_sockets::Blob;

#ifndef NDEBUG
  {
    std::cout << yellow << bold << "warning" << reset << ": unoptimized build";
#if defined(CURRENT_POSIX) || defined(CURRENT_APPLE)
    std::cout << ", run " << cyan << "NDEBUG=1 make clean all";
#endif
    std::cout << std::endl;
  }
#endif

  const size_t N = std::max(static_cast<size_t>(8u),
                            static_cast<size_t>((1e6 * FLAGS_send_buffer_mb + sizeof(Blob) - 1) / sizeof(Blob)));
  // const double real_mb = 1e-6 * N * sizeof(Blob);

  std::cout << "Format: " << (FLAGS_evensodds ? "{ all, evens, odds } * " : "")
            << "\"$(average) + $(p01) / $(p10) / $(median) / $(p90) / $(p99) + N=$(sample size)\"." << std::endl;

  current::ProgressLine progress;

  // progress << current::strings::Printf("allocating %.1lfMB", real_mb);
  std::vector<Blob> data(N);

  // progress << current::strings::Printf("initializing %.1lfMB", real_mb);
  for (Blob& b : data) {
    using namespace current::examples::streamed_sockets;
    b.request_origin = current::random::RandomUInt64(request_origin_range_lo, request_origin_range_hi);
  }

  current::WaitableAtomic<deques_t> timestamps;

  // The code to listen to the confirmations of own requests.
  std::thread t_listener([&timestamps, &progress]() {

    progress << "listening, to measure latency, on localhost:" << FLAGS_listen_port;
    current::net::Socket socket(FLAGS_listen_port);
    progress << "accepting connections, to measure latency, on localhost:" << FLAGS_listen_port;
    current::net::Connection connection(socket.Accept());
    progress << "";

    Blob blob;
    while (connection.BlockingRead(reinterpret_cast<uint8_t*>(&blob), sizeof(Blob))) {
      const int64_t t_now = current::time::Now().count();
      timestamps.MutableUse([t_now, &blob, &progress](deques_t& dqs) {
        dqs.second.emplace_back(blob.request_sequence_id, t_now);
        RelaxQueues(dqs, progress);
      });
      // std::cerr << "<< " << t_now << '\t' << blob.request_sequence_id << '\n';
    }
  });

  // The code that originate requests.
  // progress << "preparing to send";
  uint64_t request_sequence_id = 0;
  while (true) {
    try {
      current::net::Connection connection(current::net::ClientSocket(FLAGS_host, FLAGS_port));
      // progress
      //   << "connected, " << magenta << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port
      //   << reset << " => " << cyan << connection.RemoteIPAndPort().ip << ':' << connection.RemoteIPAndPort().port
      //   << reset;
      while (true) {
        const uint64_t begin_index = current::random::RandomUInt64(0, N / 4);
        const uint64_t end_index = N - current::random::RandomUInt64(0, N / 4);
        const uint64_t a = begin_index;
        const uint64_t b = end_index - 1u;
        const uint64_t request_origin_a_save = data[a].request_origin;
        const uint64_t request_origin_b_save = data[b].request_origin;
        data[a].request_origin = current::examples::streamed_sockets::request_origin_latencytest;
        data[b].request_origin = current::examples::streamed_sockets::request_origin_latencytest;
        data[a].request_sequence_id = request_sequence_id;
        data[b].request_sequence_id = request_sequence_id + 1;
        const int64_t t_write = current::time::Now().count();
        connection.BlockingWrite(
            reinterpret_cast<const void*>(&data[begin_index]), (end_index - begin_index) * sizeof(Blob), false);
        data[a].request_origin = request_origin_a_save;
        data[b].request_origin = request_origin_b_save;
        // std::cerr << ">> " << t_write << '\t' << request_sequence_id << '\n';
        // std::cerr << "<< " << t_end << '\t' << request_sequence_id + 1 << '\n';
        timestamps.MutableUse([t_write, request_sequence_id, &progress](deques_t& dqs) {
          dqs.first.emplace_back(request_sequence_id, t_write);
          dqs.first.emplace_back(request_sequence_id + 1, t_write);
          RelaxQueues(dqs, progress);
        });
        request_sequence_id += 2;
      }
    } catch (const current::net::SocketConnectException&) {
      // progress << "connecting to " << red << bold << FLAGS_host << ':' << FLAGS_port << reset;
    } catch (const current::Exception& e) {
      // progress << red << bold << "error" << reset << ": " << e.OriginalDescription() << reset;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Don't eat up 100% CPU when unable to connect.
  }

  t_listener.join();
}
