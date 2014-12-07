// Linux- and Mac-specific unit tests for TCP server implementation. Using external `telnet` from shell.
//
// TODO(dkorolev): ONLY HERE TO INITIALLY CHECK THE CODE IN. TO BE REPLACED BY C++ TCP CLIENT.
//
// Don't write unit tests like this.
// Please, never write unit tests like this.
// Every time you write a test like this God kills a kitten.
// Hear me. Don't. Just don't. It's not worth your karma. Go do something else.

#include <thread>
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

bool HasPrefix(const char* str, const char* prefix) {
  return !strncmp(str, prefix, strlen(prefix));
}

string Telnet(const string& cmdline) {
  FILE* pipe = ::popen(cmdline.c_str(), "r");
  assert(pipe);
  char s[1024];
  do {
    ::fgets(s, sizeof(s), pipe);
  } while (HasPrefix(s, "Trying ") || HasPrefix(s, "Connected to ") || HasPrefix(s, "Escape character "));
  ::pclose(pipe);
  return s;
}

TEST(Net, TelnetReceiveMessage) {
  thread t([]() { Connection(Socket(FLAGS_port).Accept()).BlockingWrite("BOOM"); });
  EXPECT_EQ("BOOM", Telnet(string("telnet localhost ") + to_string(FLAGS_port) + " 2>/dev/null"));
  t.join();
}

TEST(Net, TelnetEchoMessage) {
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

TEST(Net, TelnetEchoTwoMessages) {
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
