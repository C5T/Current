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

// The simplest possible high-throughput TCP latency measuring tool.
// Does not even check what is being returned, just assumes what is read is correct.
//
// The default flags assumes `./.current/forward` is run, and localhost:9001 is forwared back to localhost:9002.
//
// To run, first start `./.current/forward`, and then this tool. They would both terminate automatically.

#include <cstdlib>
#include <thread>

#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/time/chrono.h"

DEFINE_string(sendto_host, "localhost", "The host to send data to.");
DEFINE_uint16(sendto_port, 9002, "The port to send data to.");
DEFINE_uint16(listen_port, 9001, "The port to listen on.");

DEFINE_uint64(n, 1 << 25, "The number of bytes in each block to send.");
DEFINE_uint32(k, 20, "The number of blocks to send.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::net::Connection sendto(current::net::ClientSocket(FLAGS_sendto_host, FLAGS_sendto_port));

  current::net::Socket socket(FLAGS_listen_port);
  current::net::Connection recvfrom(socket.Accept());

  std::vector<std::chrono::microseconds> send_begin(FLAGS_k);
  std::vector<std::chrono::microseconds> send_end(FLAGS_k);
  std::atomic_size_t sent_done(0u);

  std::vector<std::chrono::microseconds> recv_begin(FLAGS_k);
  std::vector<std::chrono::microseconds> recv_end(FLAGS_k);
  std::atomic_size_t recv_done(0u);

  std::thread thread_send([&sendto, &sent_done, &send_begin, &send_end]() {
    std::vector<uint8_t> buffer(FLAGS_n);
    for (uint8_t& b : buffer) {
      b = rand();
    }
    for (size_t k = 0u; k < FLAGS_k; ++k) {
      send_begin[k] = current::time::Now();
      sendto.BlockingWrite(&buffer[0], buffer.size(), true);
      send_end[k] = current::time::Now();
      sent_done = k + 1u;
    }
  });

  std::thread thread_recv([&recvfrom, &recv_done, &recv_begin, &recv_end]() {
    std::vector<uint8_t> buffer(FLAGS_n);
    for (size_t k = 0u; k < FLAGS_k; ++k) {
      recv_begin[k] = current::time::Now();
      recvfrom.BlockingRead(&buffer[0], buffer.size(), current::net::Connection::BlockingReadPolicy::FillFullBuffer);
      recv_end[k] = current::time::Now();
      recv_done = k + 1u;
    }
  });

  std::thread thread_dump([&]() {
    const double coef = 1e-3 * FLAGS_n;  // (GB/s) == 1e-3 * (B/us).
    for (size_t k = 1u; k < FLAGS_k; ++k) {
      while (!(k < sent_done && k < recv_done)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      printf("Block %5d sent at %.3lfGB/s, received at %.3lf GB/s, average latency %.2lfms\n",
             static_cast<int>(k),
             coef / ((send_end[k] - send_begin[k]).count() + 1),
             coef / ((recv_end[k] - recv_begin[k]).count() + 1),
             1e-3 * ((recv_begin[k] + recv_end[k]).count() - (send_begin[k] + send_end[k]).count()) / 2);
    }
  });

  thread_send.join();
  thread_recv.join();
  thread_dump.join();
}
