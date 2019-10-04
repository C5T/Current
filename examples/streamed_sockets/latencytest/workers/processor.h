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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_PROCESSOR_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_PROCESSOR_H

#include <cstdlib>
#include <iostream>

#include "../blob.h"

#include "../../../../bricks/net/tcp/tcp.h"
#include "../../../../bricks/time/chrono.h"

namespace current::examples::streamed_sockets {

struct ProcessingWorker final {
  struct ProcessingWorkedImpl {
    current::net::Connection connection;
    ProcessingWorkedImpl(const std::string& host, uint16_t port) : connection(current::net::ClientSocket(host, port)) {}
  };
  std::unique_ptr<ProcessingWorkedImpl> impl;

  const std::string host;
  const uint16_t port;
  ProcessingWorker(std::string host, uint16_t port) : host(std::move(host)), port(port) {}

  uint64_t next_expected_total_index = static_cast<uint64_t>(-1);
  const Blob* DoWork(Blob* begin, Blob* end) {
    // NOTE(dkorolev): Process in blocks of the size up to 512KB.
    constexpr static size_t block_size_in_blobs = (1 << 19) / sizeof(Blob);
    static_assert(block_size_in_blobs > 0);
    if (end > begin + block_size_in_blobs) {
      end = begin + block_size_in_blobs;
    }
    while (begin != end) {
      if (next_expected_total_index == static_cast<uint64_t>(-1)) {
        next_expected_total_index = begin->index;
      } else if (begin->index != next_expected_total_index) {
        std::cerr << "Broken index continuity; exiting.\n";
        std::exit(-1);
      }
      if (begin->request_origin == request_origin_latencytest) {
        if (!impl) {
          impl = std::make_unique<ProcessingWorkedImpl>(host, port);
        }
        // NOTE(dkorolev): This call should most certainly not be a synchronous `BlockingWrite`.
        impl->connection.BlockingWrite(reinterpret_cast<const void*>(begin), sizeof(Blob), false);
      }
      ++begin;
      ++next_expected_total_index;
    }
    return begin;
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_PROCESSOR_H
