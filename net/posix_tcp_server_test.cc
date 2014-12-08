#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>

#include "posix_tcp_server.h"

#include "../dflags/dflags.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

DEFINE_int32(port, 8081, "Port to use for the test.");

using std::string;
using std::vector;
using std::function;
using std::thread;
using std::to_string;

using bricks::net::Socket;
using bricks::net::Connection;
using bricks::net::ClientSocket;

using bricks::net::SocketReadMultibyteRecordEndedPrematurelyException;

struct TCPImplTelnet {
  static string Run(std::thread& server_thread,
                    const string& host,
                    const uint16_t port,
                    function<void(Connection&)> f) {
    Connection connection(ClientSocket(host, port));
    f(connection);
    connection.SendEOF();
    server_thread.join();
    // TODO(dkorolev): Talk to Alex here; perhaps we do need some ReadTillEOF() method for Connection.
    const size_t max_size = 1000;
    vector<uint8_t> buffer(max_size);
    const size_t length = connection.BlockingRead(&buffer[0], max_size);
    return string(buffer.begin(), buffer.begin() + length);
  }
  static string Run(std::thread& server_thread,
                    const string& host,
                    const uint16_t port,
                    const string& message = "") {
    return Run(server_thread, host, port, [&message](Connection& connection) {
      if (!message.empty()) {
        connection.BlockingWrite(message);
      }
    });
  }
};

template <typename T>
class TCPTest : public ::testing::Test {};

typedef ::testing::Types<TCPImplTelnet> TestTCPImplsTypeList;
TYPED_TEST_CASE(TCPTest, TestTCPImplsTypeList);

TYPED_TEST(TCPTest, ReceiveMessage) {
  thread server([](Socket socket) { socket.Accept().BlockingWrite("BOOM"); }, std::move(Socket(FLAGS_port)));
  EXPECT_EQ("BOOM", TypeParam::Run(server, "localhost", FLAGS_port));
}

TYPED_TEST(TCPTest, ReceiveMessageOfTwoUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         socket.Accept().BlockingWrite(vector<uint16_t>{0x3031, 0x3233});
                       },
                       std::move(Socket(FLAGS_port)));
  EXPECT_EQ("1032", TypeParam::Run(server_thread, "localhost", FLAGS_port));
}

TYPED_TEST(TCPTest, EchoMessage) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         const size_t length = 7;
                         string s(length, ' ');
                         size_t offset = 0;
                         while (offset < length) {
                           const size_t bytes_read = connection.BlockingRead(&s[offset], length - offset);
                           ASSERT_GT(bytes_read, 0);
                           offset += bytes_read;
                         }
                         ASSERT_EQ(length, s.length());
                         connection.BlockingWrite("ECHO: " + s);
                       },
                       std::move(Socket(FLAGS_port)));
  EXPECT_EQ("ECHO: TEST OK", TypeParam::Run(server_thread, "localhost", FLAGS_port, "TEST OK"));
}

TYPED_TEST(TCPTest, EchoMessageOfTwoUInt16) {
  // Note: This tests endianness as well -- D.K.
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         const size_t length = 2;
                         vector<uint16_t> data(length);
                         size_t offset = 0;
                         while (offset < length) {
                           const size_t bytes_read = connection.BlockingRead(&data[offset], length - offset);
                           ASSERT_GT(bytes_read, 0);
                           offset += bytes_read;
                         }
                         ASSERT_EQ(length, data.size());
                         char s[64];
                         ::snprintf(s, sizeof(s), "UINT16-s: %04x %04x", data[0], data[1]);
                         connection.BlockingWrite(s);
                       },
                       std::move(Socket(FLAGS_port)));
  EXPECT_EQ("UINT16-s: 3252 3244", TypeParam::Run(server_thread, "localhost", FLAGS_port, "R2D2"));
}

TYPED_TEST(TCPTest, EchoTwoMessages) {
  thread server_thread([](Socket socket) {
                         Connection connection(socket.Accept());
                         for (int i = 0; i < 2; ++i) {
                           const size_t length = 3;
                           string s(length, ' ');
                           size_t offset = 0;
                           while (offset < length) {
                             const size_t bytes_read = connection.BlockingRead(&s[offset], length - offset);
                             ASSERT_GT(bytes_read, 0);
                             offset += bytes_read;
                           }
                           ASSERT_EQ(length, s.length());
                           connection.BlockingWrite(i ? "," : "ECHO2: ");
                           connection.BlockingWrite(s);
                         }
                       },
                       std::move(Socket(FLAGS_port)));
  EXPECT_EQ("ECHO2: FOO,BAR",
            TypeParam::Run(server_thread, "localhost", FLAGS_port, [](Connection& connection) {
              connection.BlockingWrite("FOO");
              connection.BlockingWrite("BAR");
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
                       std::move(Socket(FLAGS_port)));
  EXPECT_EQ("PART",
            TypeParam::Run(server_thread, "localhost", FLAGS_port, [](Connection& connection) {
              connection.BlockingWrite("FUUU");
            }));
  EXPECT_EQ('F', big_struct.first_byte);
  EXPECT_EQ('U', big_struct.second_byte);
}
