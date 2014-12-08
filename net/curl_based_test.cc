// Linux- and Mac-specific unit tests for HTTP server implementation. Using external `curl` from shell.
//
// TODO(dkorolev): ONLY HERE TO INITIALLY CHECK THE CODE IN. TO BE REPLACED BY C++ HTTP CLIENT.
//
// Don't write unit tests like this.
// Please, never write unit tests like this.
// Every time you write a test like this God kills a kitten.
// Hear me. Don't. Just don't. It's not worth your karma. Go do something else.

#include <thread>

#include "posix_http_server.h"

#include "../dflags/dflags.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

DEFINE_int32(port, 8081, "Port to use for the test.");

using std::string;
using std::thread;
using std::to_string;

using bricks::net::Socket;
using bricks::net::HTTPConnection;
using bricks::net::HTTPNoBodyProvidedException;

string Curl(const string& cmdline) {
  FILE* pipe = ::popen(cmdline.c_str(), "r");
  assert(pipe);
  char s[1024];
  ::fgets(s, sizeof(s), pipe);
  ::pclose(pipe);
  return s;
}

TEST(CurlHTTP, GET) {
  thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("GET", c.Method());
    EXPECT_EQ("/unittest", c.URL());
    c.SendHTTPResponse("PASSED");
  });
  EXPECT_EQ("PASSED", Curl(string("curl -s localhost:") + to_string(FLAGS_port) + "/unittest"));
  t.join();
}

TEST(CurlHTTP, POST) {
  thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("POST", c.Method());
    EXPECT_EQ("/unittest_post", c.URL());
    EXPECT_EQ("BAZINGA", c.Body());
    c.SendHTTPResponse("POSTED");
  });
  EXPECT_EQ("POSTED", Curl(string("curl -s -d BAZINGA localhost:") + to_string(FLAGS_port) + "/unittest_post"));
  t.join();
}

TEST(CurlHTTP, NoBodyPost) {
  thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("POST", c.Method());
    EXPECT_EQ("/unittest_empty_post", c.URL());
    EXPECT_FALSE(c.HasBody());
    ASSERT_THROW(c.Body(), HTTPNoBodyProvidedException);
    c.SendHTTPResponse("ALMOST_POSTED");
  });
  EXPECT_EQ("ALMOST_POSTED",
            Curl(string("curl -s -X POST localhost:") + to_string(FLAGS_port) + "/unittest_empty_post"));
  t.join();
}
