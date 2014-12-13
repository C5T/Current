#include <chrono>
#include <functional>
#include <thread>

#include "tcp.h"

#include "../../dflags/dflags.h"

#include "../../strings/printf.h"

#include "../../3party/gtest/gtest.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(port, 8081, "Port to use for the test.");

using std::function;
using std::move;
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

using bricks::net::SocketReadMultibyteRecordEndedPrematurelyException;

struct TCPClientImplPOSIX {
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
    return ReadFromSocket(server_thread, "localhost", FLAGS_port, client_code);
  }

  static string ReadFromSocket(thread& server_thread, const string& message_to_send_from_client = "") {
    return ReadFromSocket(server_thread, "localhost", FLAGS_port, message_to_send_from_client);
  }
};

template <typename T>
class TCPTest : public ::testing::Test {};

typedef ::testing::Types<TCPClientImplPOSIX> TCPClientImplsTypeList;
TYPED_TEST_CASE(TCPTest, TCPClientImplsTypeList);

TYPED_TEST(TCPTest, ReceiveMessage) {
  thread server([](Socket socket) { socket.Accept().BlockingWrite("BOOM"); }, move(Socket(FLAGS_port)));
  EXPECT_EQ("BOOM", TypeParam::ReadFromSocket(server));
}

TYPED_TEST(TCPTest, ReceiveMessageOfTwoUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         socket.Accept().BlockingWrite(vector<uint16_t>{0x3031, 0x3233});
                       },
                       move(Socket(FLAGS_port)));
  EXPECT_EQ("1032", TypeParam::ReadFromSocket(server_thread));
}

TYPED_TEST(TCPTest, EchoMessage) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         connection.BlockingWrite("ECHO: " + connection.BlockingReadUntilEOF());
                       },
                       move(Socket(FLAGS_port)));
  EXPECT_EQ("ECHO: TEST OK", TypeParam::ReadFromSocket(server_thread, "TEST OK"));
}

TYPED_TEST(TCPTest, EchoMessageOfThreeUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         connection.BlockingWrite("UINT16-s:");
                         for (const auto value : connection.BlockingReadUntilEOF<vector<uint16_t> >()) {
                           connection.BlockingWrite(Printf(" %04x", value));
                         }
                       },
                       move(Socket(FLAGS_port)));
  EXPECT_EQ("UINT16-s: 3252 2020 3244", TypeParam::ReadFromSocket(server_thread, "R2  D2"));
}

TYPED_TEST(TCPTest, EchoThreeMessages) {
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
                       move(Socket(FLAGS_port)));
  EXPECT_EQ("ECHO: FOO,BAR,BAZ",
            TypeParam::ReadFromSocket(server_thread, [](Connection& connection) {
              connection.BlockingWrite("FOOBARB");
              sleep_for(milliseconds(1));
              connection.BlockingWrite("A");
              sleep_for(milliseconds(1));
              connection.BlockingWrite("Z");
            }));
}

TYPED_TEST(TCPTest, PrematureMessageEndingException) {
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
                       move(Socket(FLAGS_port)));
  EXPECT_EQ("PART",
            TypeParam::ReadFromSocket(server_thread,
                                      [](Connection& connection) { connection.BlockingWrite("FUUU"); }));
  EXPECT_EQ('F', big_struct.first_byte);
  EXPECT_EQ('U', big_struct.second_byte);
}
