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

// Simple TCP traffic forwarder.
//
// Example usage A, one hop:
// 1) Start ./.current/receiver  # Listens on 9001.
// 2) Start ./.current/forward   # Forwards 9002 to 9001.
// 3) Start ./.current/sender --port 9002
//
// Example usage B, two hops:
// 1) Start ./.current/receiver  # Listens on 9001.
// 2) Start ./.current/forward   # Forwards 9002 to 9001.
// 3) Start ./.current/forward --listen_port 9003 --sendto_port 9002
// 4) Start ./.current/sender --port 9003
//
// Multiple hops can be simulated.

#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"

DEFINE_uint16(listen_port, 9002, "The port to listen on.");
DEFINE_string(sendto_host, "localhost", "The host to forward to.");
DEFINE_uint16(sendto_port, 9001, "The port to forward to.");
DEFINE_uint64(n, 1 << 20, "Buffer size, in bytes.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::net::Socket socket((current::net::BarePort(FLAGS_listen_port)));
  current::net::Connection recvfrom(socket.Accept());

  current::net::Connection sendto(current::net::ClientSocket(FLAGS_sendto_host, FLAGS_sendto_port));

  std::vector<uint8_t> buffer(FLAGS_n);
  while (true) {
    const size_t size = recvfrom.BlockingRead(&buffer[0], buffer.size());
    sendto.BlockingWrite(&buffer[0], size, true);
  }
}
