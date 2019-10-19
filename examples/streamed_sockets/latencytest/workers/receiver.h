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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_RECEIVER_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_RECEIVER_H

#include "../blob.h"

#include "../../../../bricks/net/tcp/tcp.h"
#include "../../../../bricks/time/chrono.h"

namespace current::examples::streamed_sockets {

struct ReceivingWorker final {
  struct ReceivingWorkerImpl {
    current::net::Socket socket;
    current::net::Connection connection;
    ReceivingWorkerImpl(uint16_t port) : socket(port), connection(socket.Accept()) {}
  };
  std::unique_ptr<ReceivingWorkerImpl> impl;

  const uint16_t port;
  explicit ReceivingWorker(uint16_t port) : port(port) {}

  size_t DoGetInput(uint8_t* begin, uint8_t* end) {
    // NOTE(dkorolev): My experiments show that latency is sensitive to the socket read block size,
    // and the block size of 32K is good enough, as it both doesn't hurt throughput and results in low latency.
    constexpr static size_t block_size_in_bytes = (1 << 15);
    if (end > begin + block_size_in_bytes) {
      end = begin + block_size_in_bytes;
    }
    if (!impl) {
      impl = std::make_unique<ReceivingWorkerImpl>(port);
    }
    return impl->connection.BlockingRead(begin, end - begin);
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_RECEIVER_H
