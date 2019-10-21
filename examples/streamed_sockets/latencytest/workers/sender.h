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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SENDER_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SENDER_H

#include "../blob.h"

#include "../../../../bricks/net/tcp/tcp.h"
#include "../../../../bricks/time/chrono.h"

namespace current::examples::streamed_sockets {

struct SendingWorker final {
  struct SendingWorkedImpl {
    current::net::Connection connection;
    SendingWorkedImpl(const std::string& host, uint16_t port) : connection(current::net::ClientSocket(host, port)) {}
  };
  std::unique_ptr<SendingWorkedImpl> impl;

  const std::string host;
  const uint16_t port;
  SendingWorker(std::string host, uint16_t port) : host(std::move(host)), port(port) {}

  const Blob* DoWork(const Blob* begin, const Blob* end) {
    // NOTE(dkorolev): Send in blocks of the size up to 256KB.
    constexpr static size_t block_size_in_blobs = (1 << 18) / sizeof(Blob);
    static_assert(block_size_in_blobs > 0);
    if (end > begin + block_size_in_blobs) {
      end = begin + block_size_in_blobs;
    }
    while (true) {
      if (!impl) {
        impl = std::make_unique<SendingWorkedImpl>(host, port);
      }
      impl->connection.BlockingWrite(reinterpret_cast<const void*>(begin), (end - begin) * sizeof(Blob), false);
      return end;
    }
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SENDER_H
