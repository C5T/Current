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
#include "../../util/singleton.h"

#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

using std::function;
using std::string;
using std::to_string;
using std::vector;

using std::chrono::milliseconds;
using std::this_thread::sleep_for;

using current::Singleton;
using current::net::ClientSocket;
using current::net::Connection;
using current::net::ResolveIPFromHostname;
using current::net::Socket;
using current::strings::Printf;

using current::net::AttemptedToUseMovedAwayConnection;
using current::net::SocketBindException;
using current::net::SocketException;
using current::net::SocketResolveAddressException;

using current::net::ReserveLocalPort;

DEFINE_uint16(port_hint, 27061, "The port to attempt to use.");

static void ExpectFromSocket(const std::string& golden,
                             std::thread& server_thread,
                             const string& host,
                             const uint16_t port,
                             function<void(Connection&)> client_code) {
  Connection connection(ClientSocket(host, port));
  client_code(connection);
  std::vector<char> response(golden.length());
  ASSERT_EQ(golden.length(),
            connection.BlockingRead(&response[0], golden.length(), Connection::BlockingReadPolicy::FillFullBuffer));
  EXPECT_EQ(golden, std::string(response.begin(), response.end()));
  server_thread.join();
}

static void ExpectFromSocket(const std::string& golden,
                             std::thread& server_thread,
                             const string& host,
                             const uint16_t port,
                             const string& message_to_send_from_client = "") {
  ExpectFromSocket(golden, server_thread, host, port, [&message_to_send_from_client](Connection& connection) {
    if (!message_to_send_from_client.empty()) {
      connection.BlockingWrite(message_to_send_from_client, false);
    }
  });
}

static void ExpectFromSocket(const std::string& golden,
                             std::thread& server_thread,
                             const uint16_t port,
                             const string& message_to_send_from_client = "") {
  ExpectFromSocket(golden, server_thread, "localhost", port, [&message_to_send_from_client](Connection& connection) {
    if (!message_to_send_from_client.empty()) {
      connection.BlockingWrite(message_to_send_from_client, false);
    }
  });
}

TEST(TCPTest, IPAndPort) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;

  std::thread server(
      [port_number](Socket socket) {
        Connection connection = socket.Accept();
        EXPECT_EQ("127.0.0.1", connection.LocalIPAndPort().ip);
        EXPECT_EQ(port_number, connection.LocalIPAndPort().port);
        EXPECT_EQ("127.0.0.1", connection.RemoteIPAndPort().ip);
        EXPECT_LT(0, connection.RemoteIPAndPort().port);
        connection.BlockingWrite("42", false);
      },
      Socket(std::move(port_reservation)));
  Connection client(ClientSocket("127.0.0.1", port_number));
  EXPECT_EQ("127.0.0.1", client.LocalIPAndPort().ip);
  EXPECT_LT(0, client.LocalIPAndPort().port);
  EXPECT_EQ("127.0.0.1", client.RemoteIPAndPort().ip);
  EXPECT_EQ(port_number, client.RemoteIPAndPort().port);
  char response[2];
  ASSERT_EQ(2u, client.BlockingRead(response, 2, Connection::FillFullBuffer));
  EXPECT_EQ("42", std::string(response, 2));
  server.join();
}

TEST(TCPTest, ReceiveMessage) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server([](Socket socket) { socket.Accept().BlockingWrite("BOOM", false); }, std::move(port_reservation));
  ExpectFromSocket("BOOM", server, port_number);
}

TEST(TCPTest, ReceiveMessageOfTwoParts) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server([](Socket socket) { socket.Accept().BlockingWrite("BOO", true).BlockingWrite("M", false); },
                     std::move(port_reservation));
  ExpectFromSocket("BOOM", server, port_number);
}

TEST(TCPTest, ReceiveDelayedMessage) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server(
      [](Socket socket) {
        Connection connection = socket.Accept();
        connection.BlockingWrite("BLA", true);
        sleep_for(milliseconds(1));
        connection.BlockingWrite("H", false);
      },
      std::move(port_reservation));
  Connection client(ClientSocket("localhost", port_number));
  char response[5] = "????";
  ASSERT_EQ(4u, client.BlockingRead(response, 4, Connection::FillFullBuffer));
  EXPECT_EQ("BLAH", std::string(response));
  server.join();
}

TEST(TCPTest, ReceiveMessageAsResponse) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server(
      [](Socket socket) {
        Connection connection = socket.Accept();
        connection.BlockingWrite("{", true);
        char buffer[2];  // Wait for three incoming bytes before sending data out.
        ASSERT_EQ(2u, connection.BlockingRead(buffer, 2, Connection::FillFullBuffer));
        connection.BlockingWrite(buffer, 2, true);
        connection.BlockingWrite("}", false);
      },
      std::move(port_reservation));
  Connection client(ClientSocket("localhost", port_number));
  client.BlockingWrite("!?", false);
  char response[5] = "????";
  ASSERT_EQ(4u, client.BlockingRead(response, 4, Connection::FillFullBuffer));
  EXPECT_EQ("{!?}", std::string(response));
  server.join();
}

TEST(TCPTest, CanNotUseMovedAwayConnection) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server(
      [](Socket socket) {
        Connection connection = socket.Accept();
        char buffer[3];  // Wait for three incoming bytes before sending data out.
        ASSERT_EQ(3u, connection.BlockingRead(buffer, 3, Connection::FillFullBuffer));
        connection.BlockingWrite("OK", false);
      },
      std::move(port_reservation));
  Connection old_connection(ClientSocket("localhost", port_number));
  old_connection.BlockingWrite("1", true);
  Connection new_connection(std::move(old_connection));
  ASSERT_THROW(old_connection.BlockingWrite("333", true), AttemptedToUseMovedAwayConnection);
  char tmp[3];
  ASSERT_THROW(old_connection.BlockingRead(tmp, 3), AttemptedToUseMovedAwayConnection);
  new_connection.BlockingWrite("22", false);
  char response[3] = "WA";
  ASSERT_EQ(2u, new_connection.BlockingRead(response, 2, Connection::FillFullBuffer));
  EXPECT_EQ("OK", std::string(response));
  server.join();
}

TEST(TCPTest, ReceiveMessageOfTwoUInt16AndTestEndianness) {
  // NOTE(dkorolev): This tests endianness as well.
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server_thread(
      [](Socket socket) {
        socket.Accept().BlockingWrite(vector<uint16_t>{0x3031, 0x3233}, false);
      },
      std::move(port_reservation));
  ExpectFromSocket("1032", server_thread, port_number);
}

TEST(TCPTest, EchoMessage) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server_thread(
      [](Socket socket) {
        Connection connection(socket.Accept());
        std::vector<char> s(7);
        ASSERT_EQ(s.size(), connection.BlockingRead(&s[0], s.size(), Connection::FillFullBuffer));
        connection.BlockingWrite("ECHO: " + std::string(s.begin(), s.end()), false);
      },
      std::move(port_reservation));
  ExpectFromSocket("ECHO: TEST OK", server_thread, port_number, std::string("TEST OK"));
}

TEST(TCPTest, EchoThreeMessages) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server_thread(
      [](Socket socket) {
        const size_t block_length = 3;
        string s(block_length, ' ');
        Connection connection(socket.Accept());
        for (int i = 0; i < 3; ++i) {
          ASSERT_EQ(block_length, connection.BlockingRead(&s[0], block_length, Connection::FillFullBuffer));
          connection.BlockingWrite(i > 0 ? "," : "ECHO: ", true);
          connection.BlockingWrite(s, false);
        }
      },
      std::move(port_reservation));
  ExpectFromSocket("ECHO: FOO,BAR,BAZ", server_thread, "localhost", port_number, [](Connection& connection) {
    connection.BlockingWrite("FOOBARB", true);
    sleep_for(milliseconds(1));
    connection.BlockingWrite("A", true);
    sleep_for(milliseconds(1));
    connection.BlockingWrite("Z", false);
  });
}

TEST(TCPTest, EchoLongMessageTestsDynamicBufferGrowth) {
  current::net::ReservedLocalPort port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server_thread(
      [](Socket socket) {
        Connection connection(socket.Accept());
        std::vector<char> s(10000);
        ASSERT_EQ(s.size(), connection.BlockingRead(&s[0], s.size(), Connection::FillFullBuffer));
        connection.BlockingWrite("ECHO: " + std::string(s.begin(), s.end()), false);
      },
      std::move(port_reservation));
  std::string message;
  for (size_t i = 0; i < 10000; ++i) {
    message += '0' + (i % 10);
  }
  EXPECT_EQ(10000u, message.length());
  ExpectFromSocket("ECHO: " + message, server_thread, "localhost", port_number, message);
}

// Don't run this ~1s test more than once in a loop.
struct RunResolveAddressTestOnlyOnceSingleton {
  bool done = false;
};

// NOTE: This test should pass without active internet connection.
TEST(TCPTest, ResolveAddress) {
  bool& done = Singleton<RunResolveAddressTestOnlyOnceSingleton>().done;
  if (!done) {
    EXPECT_EQ("127.0.0.1", ResolveIPFromHostname("localhost"));
    EXPECT_EQ("127.0.0.1", ResolveIPFromHostname("127.0.0.1"));
    EXPECT_EQ("9.8.7.6", ResolveIPFromHostname("9.8.7.6"));
    try {
      ResolveIPFromHostname("someunknownhostname.domain");
      ASSERT_TRUE(false);
    } catch (SocketResolveAddressException& e) {
      EXPECT_EQ("someunknownhostname.domain", e.OriginalDescription());
    }
    try {
      ClientSocket("999.999.999.999", 80);
      ASSERT_TRUE(false);
    } catch (SocketResolveAddressException& e) {
      EXPECT_EQ("999.999.999.999 80", e.OriginalDescription());
    }
    done = true;
  }
}

TEST(TCPTest, CanNotBindTwoSocketsToTheSamePortSimultaneously) {
  auto port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  Socket s1(std::move(port_reservation));
  std::unique_ptr<Socket> s2;
  ASSERT_THROW(s2 = std::make_unique<Socket>(current::net::BarePort(port_number)), SocketBindException);
}

#if !defined(CURRENT_WINDOWS) && !defined(CURRENT_APPLE)
// NOTE: This test appears to be flaky.
// Apparently, Windows has no problems sending a 10MiB message -- D.K.
// Tested on Visual Studio 2015 Preview.
// Temporary disabled for Apple -- M.Z.
TEST(TCPTest, WriteExceptionWhileWritingAVeryLongMessage) {
  auto port_reservation = ReserveLocalPort();
  const uint16_t port_number = port_reservation;
  std::thread server_thread(
      [](Socket socket) {
        Connection connection(socket.Accept());
        char buffer[3];
        connection.BlockingRead(buffer, 3, Connection::FillFullBuffer);
        connection.BlockingWrite("Done, thanks.\n", false);
      },
      Socket(std::move(port_reservation)));
  // Attempt to send a very long message to ensure it does not fit OS buffers.
  Connection connection(ClientSocket("localhost", port_number));
  ASSERT_THROW(connection.BlockingWrite(std::vector<char>(100 * 1000 * 1000, '!'), true),
               SocketException);  // NOTE(dkorolev): Catching the top-level `SocketException` to be safe.
  server_thread.join();
}
#endif

TEST(TCPTest, PickLocalPort) {
  uint16_t i1;
  uint16_t i2;
  {
    const auto p1 = ReserveLocalPort();
    i1 = p1;
    EXPECT_GE(i1, 25000u);
    EXPECT_LE(i1, 29000u);
  }
  {
    const auto p2 = ReserveLocalPort();
    i2 = p2;
    EXPECT_GE(i2, 25000u);
    EXPECT_LE(i2, 29000u);
  }
  EXPECT_NE(i1, i2);
  {
    const auto p3 = ReserveLocalPort();
    const auto p4 = ReserveLocalPort();
    EXPECT_NE(static_cast<uint16_t>(p3), static_cast<uint16_t>(p4));
  }
}

#if 0
#NOTE(dkorolev) : This test will fail if the following command is run from two terminals concurrently.
while true ; do ./.current/test \
  --gtest_filter=TCPTest.PickLocalPortWithHint \
  --gtest_throw_on_failure \
  --gtest_catch_exceptions=0 || break ; done
#The above test, `TCPTest.PickLocalPort`, would run just fine in such a scenario.
#endif

TEST(TCPTest, PickLocalPortWithHint) {
  uint16_t i1;
  uint16_t i2;
  const auto p1 = ReserveLocalPort(FLAGS_port_hint);
  i1 = p1;
  EXPECT_EQ(i1, FLAGS_port_hint) << "NOTE(dkorolev): This can fail if run in a multithreaded fashion.";
  const auto p2 = ReserveLocalPort(FLAGS_port_hint);
  i2 = p2;
  EXPECT_NE(i2, FLAGS_port_hint);
  EXPECT_NE(i1, i2);
  const auto p3 = ReserveLocalPort();
  const auto p4 = ReserveLocalPort();
  EXPECT_NE(static_cast<uint16_t>(p3), static_cast<uint16_t>(p4));
  EXPECT_NE(static_cast<uint16_t>(p3), i1);
  EXPECT_NE(static_cast<uint16_t>(p3), i2);
}
