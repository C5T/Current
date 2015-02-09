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
using bricks::net::SocketReadMultibyteRecordEndedPrematurelyException;
using bricks::net::SocketCouldNotWriteEverythingException;
using bricks::net::SocketResolveAddressException;

static string ReadFromSocket(thread& server_thread,
                             const string& host,
                             const uint16_t port,
                             function<void(Connection&)> client_code) {
  Connection connection(ClientSocket(host, port));
  client_code(connection);
  connection.SendEOF();
  server_thread.join();
  return connection.BlockingReadUntilEOF();
}

static string ReadFromSocket(thread& server_thread,
                             const string& host,
                             const uint16_t port,
                             const string& message_to_send_from_client = "") {
  return ReadFromSocket(server_thread, host, port, [&message_to_send_from_client](Connection& connection) {
    if (!message_to_send_from_client.empty()) {
      connection.BlockingWrite(message_to_send_from_client);
    }
  });
}

static string ReadFromSocket(thread& server_thread, function<void(Connection&)> client_code) {
  return ReadFromSocket(server_thread, "localhost", FLAGS_net_tcp_test_port, client_code);
}

static string ReadFromSocket(thread& server_thread, const string& message_to_send_from_client = "") {
  return ReadFromSocket(server_thread, "localhost", FLAGS_net_tcp_test_port, message_to_send_from_client);
}

using bricks::net::SocketWSAStartupException;
TEST(TCPTest, ReceiveMessage) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
		BRICKS_THROW(SocketWSAStartupException());
	}
	//	Socket s(FLAGS_net_tcp_test_port);
	thread server([](Socket socket) { socket.Accept().BlockingWrite("BOOM"); }, Socket(FLAGS_net_tcp_test_port));
  EXPECT_EQ("BOOM", ReadFromSocket(server));
}

/*
TEST(TCPTest, CanNotUseMovedAwayConnection) {
  thread server([](Socket socket) { socket.Accept().BlockingWrite("OK"); }, Socket(FLAGS_net_tcp_test_port));
  Connection old_connection(ClientSocket("localhost", FLAGS_net_tcp_test_port));
  old_connection.BlockingWrite("foo\n");
//  Connection new_connection(std::move(old_connection));
  Connection& new_connection(old_connection);
  ///new_connection.BlockingWrite("bar\n");
//  ASSERT_THROW(old_connection.BlockingWrite("baz\n"), AttemptedToUseMovedAwayConnection);
//  ASSERT_THROW(old_connection.BlockingReadUntilEOF(), AttemptedToUseMovedAwayConnection);
  new_connection.SendEOF();
  server.join();
  EXPECT_EQ("OK", new_connection.BlockingReadUntilEOF());
}
*/

TEST(TCPTest, ReceiveMessageOfTwoUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         socket.Accept().BlockingWrite(vector<uint16_t>{0x3031, 0x3233});
                       },
                       Socket(FLAGS_net_tcp_test_port));
  EXPECT_EQ("1032", ReadFromSocket(server_thread));
}

TEST(TCPTest, EchoMessage) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         connection.BlockingWrite("ECHO: " + connection.BlockingReadUntilEOF());
                       },
                       Socket(FLAGS_net_tcp_test_port));
  EXPECT_EQ("ECHO: TEST OK", ReadFromSocket(server_thread, std::string("TEST OK")));
}

TEST(TCPTest, EchoMessageOfThreeUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         connection.BlockingWrite("UINT16-s:");
                         for (const auto value : connection.BlockingReadUntilEOF<vector<uint16_t> >()) {
                           connection.BlockingWrite(Printf(" %04x", value));
                         }
                       },
                       Socket(FLAGS_net_tcp_test_port));
  EXPECT_EQ("UINT16-s: 3252 2020 3244", ReadFromSocket(server_thread, std::string("R2  D2")));
}

TEST(TCPTest, EchoThreeMessages) {
  thread server_thread([](Socket socket) {
                         const size_t block_length = 3;
                         string s(block_length, ' ');
                         Connection connection(socket.Accept());
                         for (int i = 0;; ++i) {
                           const size_t read_length =
                               connection.BlockingRead(&s[0], block_length, Connection::FillFullBuffer);
                           if (read_length) {
                             EXPECT_EQ(block_length, read_length);
                             connection.BlockingWrite(i > 0 ? "," : "ECHO: ");
                             connection.BlockingWrite(s);
                           } else {
                             break;
                           }
                         }
                       },
                       Socket(FLAGS_net_tcp_test_port));
  EXPECT_EQ("ECHO: FOO,BAR,BAZ",
            ReadFromSocket(server_thread, [](Connection& connection) {
              connection.BlockingWrite("FOOBARB");
              sleep_for(milliseconds(1));
              connection.BlockingWrite("A");
              sleep_for(milliseconds(1));
              connection.BlockingWrite("Z");
            }));
}

TEST(TCPTest, PrematureMessageEndingException) {
  struct BigStruct {
    char first_byte;
    char second_byte;
    int x[1000];
  } big_struct;
  thread server_thread([&big_struct](Socket socket) {
                         Connection connection(socket.Accept());
                         ASSERT_THROW(connection.BlockingRead(&big_struct, sizeof(big_struct)),
                                      SocketReadMultibyteRecordEndedPrematurelyException);
                         connection.BlockingWrite("PART");
                       },
                       Socket(FLAGS_net_tcp_test_port));
  EXPECT_EQ("PART",
            ReadFromSocket(server_thread, [](Connection& connection) { connection.BlockingWrite("FUUU"); }));
  EXPECT_EQ('F', big_struct.first_byte);
  EXPECT_EQ('U', big_struct.second_byte);
}

#ifndef BRICKS_WINDOWS
// Two sockets on the same port somehow get created on Windows. TODO(dkorolev): Investigate.
TEST(TCPTest, CanNotBindTwoSocketsToTheSamePortSimultaneously) {
  Socket s1(FLAGS_net_tcp_test_port);
  std::unique_ptr<Socket> s2;
  ASSERT_THROW(s2.reset(new Socket(FLAGS_net_tcp_test_port)), SocketBindException);
}
#endif

TEST(TCPTest, EchoLongMessageTestsDynamicBufferGrowth) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         connection.BlockingWrite("ECHO: " + connection.BlockingReadUntilEOF());
                       },
                       Socket(FLAGS_net_tcp_test_port));
  std::string message;
  for (size_t i = 0; i < 10000; ++i) {
    message += '0' + (i % 10);
  }
  EXPECT_EQ(10000u, message.length());
  const std::string response = ReadFromSocket(server_thread, message);
  EXPECT_EQ(10006u, response.length());
  EXPECT_EQ("ECHO: 0123", response.substr(0, 10));
  EXPECT_EQ("56789", response.substr(10006 - 5));
}

#ifndef BRICKS_WINDOWS
// Large write passes on Windows. TODO(dkorolev): Investigate.
TEST(TCPTest, WriteExceptionWhileWritingAVeryLongMessage) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         char buffer[3];
                         connection.BlockingRead(buffer, 3, Connection::FillFullBuffer);
                         connection.BlockingWrite("Done, thanks.\n");
                       },
                       Socket(FLAGS_net_tcp_test_port));
  // Attempt to send a very long message to ensure it does not fit OS buffers.
  Connection connection(ClientSocket("localhost", FLAGS_net_tcp_test_port));
  ASSERT_THROW(connection.BlockingWrite(std::vector<char>(10 * 1000 * 1000, '!')),
               SocketCouldNotWriteEverythingException);
  server_thread.join();
}
#endif

TEST(TCPTest, ResolveAddressException) {
  ASSERT_THROW(Connection(ClientSocket("999.999.999.999", 80)), SocketResolveAddressException);
}