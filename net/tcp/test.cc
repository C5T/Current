/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "tcp.h"

#include "../../dflags/dflags.h"

#include "../../strings/printf.h"

#include "../../3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(net_tcp_test_port, 8081, "Port to use for the test.");

using std::function;
using std::string;
using std::thread;
using std::to_string;
using std::vector;

using std::this_thread::sleep_for;
using std::chrono::milliseconds;

using bricks::strings::Printf;
using bricks::net::Socket;
using bricks::net::Connection;
using bricks::net::ClientSocket;

using bricks::net::AttemptedToUseMovedAwayConnection;
using bricks::net::SocketBindException;
using bricks::net::SocketCouldNotWriteEverythingException;
using bricks::net::SocketResolveAddressException;

static void ExpectFromSocket(const std::string& golden,
                             thread& server_thread,
                             const string& host,
                             const uint16_t port,
                             function<void(Connection&)> client_code) {
  Connection connection(ClientSocket(host, port));
  client_code(connection);
  std::vector<char> response(golden.length());
  ASSERT_EQ(
      golden.length(),
      connection.BlockingRead(&response[0], golden.length(), Connection::BlockingReadPolicy::FillFullBuffer));
  EXPECT_EQ(golden, std::string(response.begin(), response.end()));
  server_thread.join();
}

static void ExpectFromSocket(const std::string& golden,
                             thread& server_thread,
                             const string& host,
                             const uint16_t port,
                             const string& message_to_send_from_client = "") {
  ExpectFromSocket(golden, server_thread, host, port, [&message_to_send_from_client](Connection& connection) {
    if (!message_to_send_from_client.empty()) {
      connection.BlockingWrite(message_to_send_from_client, true);
    }
  });
}

static void ExpectFromSocket(const std::string& golden,
                             thread& server_thread,
                             function<void(Connection&)> client_code) {
  ExpectFromSocket(golden, server_thread, "localhost", FLAGS_net_tcp_test_port, client_code);
}

static void ExpectFromSocket(const std::string& golden,
                             thread& server_thread,
                             const string& message_to_send_from_client = "") {
  ExpectFromSocket(golden, server_thread, "localhost", FLAGS_net_tcp_test_port, message_to_send_from_client);
}

TEST(TCPTest, ReceiveMessage) {
  thread server([](Socket socket) { socket.Accept().BlockingWrite("BOOM", true); },
                Socket(FLAGS_net_tcp_test_port));
  ExpectFromSocket("BOOM", server);
}

TEST(TCPTest, ReceiveMessageOfTwoParts) {
  thread server([](Socket socket) { socket.Accept().BlockingWrite("BOO", false).BlockingWrite("M", true); },
                Socket(FLAGS_net_tcp_test_port));
  ExpectFromSocket("BOOM", server);
}

TEST(TCPTest, ReceiveDelayedMessage) {
  thread server([](Socket socket) {
                  Connection connection = socket.Accept();
                  connection.BlockingWrite("BLA", false);
                  sleep_for(milliseconds(1));
                  connection.BlockingWrite("H", true);
                },
                Socket(FLAGS_net_tcp_test_port));
  Connection client(ClientSocket("localhost", FLAGS_net_tcp_test_port));
  char response[5] = "????";
  ASSERT_EQ(4u, client.BlockingRead(response, 4, Connection::FillFullBuffer));
  EXPECT_EQ("BLAH", std::string(response));
  server.join();
}

TEST(TCPTest, ReceiveMessageAsResponse) {
  thread server([](Socket socket) {
                  Connection connection = socket.Accept();
                  connection.BlockingWrite("{", false);
                  char buffer[2];  // Wait for three incoming bytes before sending data out.
                  ASSERT_EQ(2u, connection.BlockingRead(buffer, 2, Connection::FillFullBuffer));
                  connection.BlockingWrite(buffer, 2, false);
                  connection.BlockingWrite("}", true);
                },
                Socket(FLAGS_net_tcp_test_port));
  Connection client(ClientSocket("localhost", FLAGS_net_tcp_test_port));
  client.BlockingWrite("!?", true);
  char response[5] = "????";
  ASSERT_EQ(4u, client.BlockingRead(response, 4, Connection::FillFullBuffer));
  EXPECT_EQ("{!?}", std::string(response));
  server.join();
}

// TODO(dkorolev): Sometimes this test takes 200ms, figure out why.
TEST(TCPTest, CanNotUseMovedAwayConnection) {
  thread server([](Socket socket) {
                  Connection connection = socket.Accept();
                  char buffer[2];  // Wait for three incoming bytes before sending data out.
                  ASSERT_EQ(2u, connection.BlockingRead(buffer, 2, Connection::FillFullBuffer));
                  connection.BlockingWrite("OK", true);
                },
                Socket(FLAGS_net_tcp_test_port));
  Connection old_connection(ClientSocket("localhost", FLAGS_net_tcp_test_port));
  old_connection.BlockingWrite("1", false);
  Connection new_connection(std::move(old_connection));
  ASSERT_THROW(old_connection.BlockingWrite("333", false), AttemptedToUseMovedAwayConnection);
  char tmp[3];
  ASSERT_THROW(old_connection.BlockingRead(tmp, 3), AttemptedToUseMovedAwayConnection);
  new_connection.BlockingWrite("2", true);
  char response[3] = "WA";
  ASSERT_EQ(2u, new_connection.BlockingRead(response, 2, Connection::FillFullBuffer));
  EXPECT_EQ("OK", std::string(response));
  server.join();
}

TEST(TCPTest, ReceiveMessageOfTwoUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         socket.Accept().BlockingWrite(vector<uint16_t>{0x3031, 0x3233}, true);
                       },
                       Socket(FLAGS_net_tcp_test_port));
  ExpectFromSocket("1032", server_thread);
}

// TODO(dkorolev): Sometimes this test takes 200ms, figure out why.
TEST(TCPTest, EchoMessage) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         std::vector<char> s(7);
                         ASSERT_EQ(s.size(),
                                   connection.BlockingRead(&s[0], s.size(), Connection::FillFullBuffer));
                         connection.BlockingWrite("ECHO: " + std::string(s.begin(), s.end()), true);
                       },
                       Socket(FLAGS_net_tcp_test_port));
  ExpectFromSocket("ECHO: TEST OK", server_thread, std::string("TEST OK"));
}

TEST(TCPTest, EchoThreeMessages) {
  thread server_thread([](Socket socket) {
                         const size_t block_length = 3;
                         string s(block_length, ' ');
                         Connection connection(socket.Accept());
                         for (int i = 0; i < 3; ++i) {
                           ASSERT_EQ(block_length,
                                     connection.BlockingRead(&s[0], block_length, Connection::FillFullBuffer));
                           connection.BlockingWrite(i > 0 ? "," : "ECHO: ", false);
                           connection.BlockingWrite(s, true);
                         }
                       },
                       Socket(FLAGS_net_tcp_test_port));
  ExpectFromSocket("ECHO: FOO,BAR,BAZ", server_thread, [](Connection& connection) {
    connection.BlockingWrite("FOOBARB", false);
    sleep_for(milliseconds(1));
    connection.BlockingWrite("A", false);
    sleep_for(milliseconds(1));
    connection.BlockingWrite("Z", true);
  });
}

TEST(TCPTest, EchoLongMessageTestsDynamicBufferGrowth) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         std::vector<char> s(10000);
                         ASSERT_EQ(s.size(),
                                   connection.BlockingRead(&s[0], s.size(), Connection::FillFullBuffer));
                         connection.BlockingWrite("ECHO: " + std::string(s.begin(), s.end()), true);
                       },
                       Socket(FLAGS_net_tcp_test_port));
  std::string message;
  for (size_t i = 0; i < 10000; ++i) {
    message += '0' + (i % 10);
  }
  EXPECT_EQ(10000u, message.length());
  ExpectFromSocket("ECHO: " + message, server_thread, message);
}

TEST(TCPTest, ResolveAddressException) {
  ASSERT_THROW(Connection(ClientSocket("999.999.999.999", 80)), SocketResolveAddressException);
}

#ifndef BRICKS_WINDOWS
// Apparently, Windows has no problems opening two sockets on the same port -- D.K.
// Tested on Visual Studio 2015 Preview.
TEST(TCPTest, CanNotBindTwoSocketsToTheSamePortSimultaneously) {
  Socket s1(FLAGS_net_tcp_test_port);
  std::unique_ptr<Socket> s2;
  ASSERT_THROW(s2.reset(new Socket(FLAGS_net_tcp_test_port)), SocketBindException);
}
#endif

#ifndef BRICKS_WINDOWS
// Apparently, Windows has no problems sending a 10MiB message -- D.K.
// Tested on Visual Studio 2015 Preview.
TEST(TCPTest, WriteExceptionWhileWritingAVeryLongMessage) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         char buffer[3];
                         connection.BlockingRead(buffer, 3, Connection::FillFullBuffer);
                         connection.BlockingWrite("Done, thanks.\n", true);
                       },
                       Socket(FLAGS_net_tcp_test_port));
  // Attempt to send a very long message to ensure it does not fit OS buffers.
  Connection connection(ClientSocket("localhost", FLAGS_net_tcp_test_port));
  ASSERT_THROW(connection.BlockingWrite(std::vector<char>(10 * 1000 * 1000, '!'), true),
               SocketCouldNotWriteEverythingException);
  server_thread.join();
}
#endif
