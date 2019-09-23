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

#include "blob.h"

#include "../../../blocks/xterm/progress.h"
#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/strings/printf.h"
#include "../../../bricks/time/chrono.h"
#include "../../../bricks/util/random.h"

DEFINE_string(host, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port, 9010, "The destination port to send data to.");
DEFINE_double(send_buffer_mb, 5.0, "Send buffer size.");

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
  const double real_mb = 1e-6 * N * sizeof(Blob);

  current::ProgressLine progress;

  progress << current::strings::Printf("allocating %.1lfMB", real_mb);
  std::vector<Blob> data(N);

  progress << current::strings::Printf("initializing %.1lfMB", real_mb);
  for (Blob& b : data) {
    using namespace current::examples::streamed_sockets;
    b.request_origin = current::random::RandomUInt64(request_origin_range_lo, request_origin_range_hi);
  }

  progress << "preparing to send";
  uint64_t request_sequence_id = 0;
  while (true) {
    try {
      current::net::Connection connection(current::net::ClientSocket(FLAGS_host, FLAGS_port));
      progress << "connected, " << magenta << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port
               << reset << " => " << cyan << connection.RemoteIPAndPort().ip << ':' << connection.RemoteIPAndPort().port
               << reset;
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
        data[b].request_sequence_id = request_sequence_id + 1u;
        // const int64_t t_begin = current::time::Now().count();
        connection.BlockingWrite(
            reinterpret_cast<const void*>(&data[begin_index]), (end_index - begin_index) * sizeof(Blob), true);
        // const int64_t t_end = current::time::Now().count();
        data[a].request_origin = request_origin_a_save;
        data[b].request_origin = request_origin_b_save;
        // std::cerr << t_begin << '\t' << request_sequence_id << '\n';
        // std::cerr << t_end << '\t' << request_sequence_id + 1 << '\n';
        request_sequence_id += 2;
      }
    } catch (const current::net::SocketConnectException&) {
      progress << "connecting to " << red << bold << FLAGS_host << ':' << FLAGS_port << reset;
    } catch (const current::Exception& e) {
      progress << red << bold << "error" << reset << ": " << e.OriginalDescription() << reset;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Don't eat up 100% CPU when unable to connect.
  }
}
