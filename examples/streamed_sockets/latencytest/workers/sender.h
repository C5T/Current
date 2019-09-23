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

template <typename = void>
struct SendingWorker {
  struct SendingWorkedImpl {
    current::net::Connection connection;
    SendingWorkedImpl(const std::string& host, uint16_t port) : connection(current::net::ClientSocket(host, port)) {}
  };
  std::unique_ptr<SendingWorkedImpl> impl;

  const std::string host;
  const uint16_t port;
  SendingWorker(std::string host, uint16_t port) : host(std::move(host)), port(port) {}

  const Blob* DoWork(Blob* const begin, Blob* const end) {
    while (true) {
      try {
        if (!impl) {
          impl = std::make_unique<SendingWorkedImpl>(host, port);
        }
        impl->connection.BlockingWrite(reinterpret_cast<const void*>(begin), (end - begin) * sizeof(Blob), true);
        return end;
      } catch (const current::net::SocketException&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Don't eat up 100% CPU when unable to connect.
      } catch (const current::Exception&) {
      }
      impl = nullptr;
      return begin;
    }
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SENDER_H
