// Linux- and Mac-specific unit tests for TCP server implementation. Using external `telnet` from shell.
//
// TODO(dkorolev): ONLY HERE TO INITIALLY CHECK THE CODE IN. TO BE REPLACED BY C++ TCP CLIENT.
//
// Don't write unit tests like this.
// Please, never write unit tests like this.
// Every time you write a test like this God kills a kitten.
// Hear me. Don't. Just don't. It's not worth your karma. Go do something else.

#include <thread>
#include <cstdio>
#include <cstring>

#include "posix_tcp_server.h"

#include "../dflags/dflags.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

DEFINE_int32(port, 8081, "Port to use for the test.");

using std::string;
using std::thread;
using std::to_string;

using ::bricks::net::Socket;
using ::bricks::net::Connection;

using ::bricks::net::SocketReadMultibyteRecordEndedPrematurelyException;

bool HasPrefix(const char* str, const char* prefix) {
  return !strncmp(str, prefix, strlen(prefix));
}

string Telnet(const string& cmdline, bool wait_for_response = true) {
  if (wait_for_response) {
    FILE* pipe = ::popen(cmdline.c_str(), "r");
    assert(pipe);
    char s[1024];
    do {
      ::fgets(s, sizeof(s), pipe);
    } while ((HasPrefix(s, "Trying ") || HasPrefix(s, "Connected to ") || HasPrefix(s, "Escape character ")));
    ::pclose(pipe);
    return s;
  } else {
    system(cmdline.c_str());
    return "";
  }
}

TEST(TelnetTCP, ReceiveMessage) {
  thread t([]() { Connection(Socket(FLAGS_port).Accept()).BlockingWrite("BOOM"); });
  EXPECT_EQ("BOOM", Telnet(string("telnet localhost ") + to_string(FLAGS_port) + " 2>/dev/null"));
  t.join();
}

TEST(TelnetTCP, ReceiveMessageOfTwoUInt16) {
  thread t([]() {
    Connection(Socket(FLAGS_port).Accept()).BlockingWrite(std::vector<uint16_t>{0x3031, 0x3233});
  });
  // Note: This tests endianness as well -- D.K.
  EXPECT_EQ("1032", Telnet(string("telnet localhost ") + to_string(FLAGS_port) + " 2>/dev/null"));
  t.join();
}

TEST(TelnetTCP, EchoMessage) {
  thread t([]() {
    Connection c(Socket(FLAGS_port).Accept());
    const size_t length = 7;
    string s(length, ' ');
    size_t offset = 0;
    while (offset < length) {
      const size_t bytes_read = c.BlockingRead(&s[offset], length - offset);
      ASSERT_GT(bytes_read, 0);
      offset += bytes_read;
    }
    ASSERT_EQ(length, s.length());
    c.BlockingWrite("ECHO: " + s);
  });
  EXPECT_EQ("ECHO: TEST OK",
            Telnet(string("(echo TEST OK ; sleep 0.1) | telnet localhost ") + to_string(FLAGS_port) +
                   " 2>/dev/null"));
  t.join();
}

TEST(TelnetTCP, EchoMessageOfTwoUInt16) {
  thread t([]() {
    Connection c(Socket(FLAGS_port).Accept());
    const size_t length = 2;
    std::vector<uint16_t> data(length);
    size_t offset = 0;
    while (offset < length) {
      const size_t bytes_read = c.BlockingRead(&data[offset], length - offset);
      ASSERT_GT(bytes_read, 0);
      offset += bytes_read;
    }
    ASSERT_EQ(length, data.size());
    // Note: This tests endianness as well -- D.K.
    char s[64];
    ::snprintf(s, sizeof(s), "UINT16-s: %04x %04x", data[0], data[1]);
    c.BlockingWrite(s);
  });
  EXPECT_EQ(
      "UINT16-s: 3252 3244",
      Telnet(string("(echo R2D2 ; sleep 0.1) | telnet localhost ") + to_string(FLAGS_port) + " 2>/dev/null"));
  t.join();
}

TEST(TelnetTCP, EchoTwoMessages) {
  thread t([]() {
    Connection c(Socket(FLAGS_port).Accept());
    for (int i = 0; i < 2; ++i) {
      const size_t length = 3;
      string s(length, ' ');
      size_t offset = 0;
      while (offset < length) {
        const size_t bytes_read = c.BlockingRead(&s[offset], length - offset);
        ASSERT_GT(bytes_read, 0);
        offset += bytes_read;
      }
      ASSERT_EQ(length, s.length());
      c.BlockingWrite(i ? "," : "ECHO2: ");
      c.BlockingWrite(s);
    }
  });
  EXPECT_EQ("ECHO2: FOO,BAR",
            Telnet(string("(echo -n FOO ; sleep 0.1 ; echo -n BAR ; sleep 0.1) | telnet localhost ") +
                   to_string(FLAGS_port) + " 2>/dev/null"));
  t.join();
}

TEST(TelnetTCP, PrematureMessageEndingException) {
  struct BigStruct {
    char first_byte;
    char second_byte;
    int x[1000];
  } big_struct;
  thread t([&big_struct]() {
    Connection c(Socket(FLAGS_port).Accept());
    ASSERT_THROW(c.BlockingRead(&big_struct, sizeof(big_struct)),
                 SocketReadMultibyteRecordEndedPrematurelyException);
  });
  Telnet(string("(echo FUUUUU ; sleep 0.1) | telnet localhost ") + to_string(FLAGS_port) +
             " >/dev/null 2>/dev/null",
         false);
  EXPECT_EQ('F', big_struct.first_byte);
  EXPECT_EQ('U', big_struct.second_byte);
  t.join();
}
