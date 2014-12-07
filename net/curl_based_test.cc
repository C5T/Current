// Linux- and Mac- specific unit tests for HTTP server implementation. Using external `curl` from shell.
//
// TODO(dkorolev): ONLY HERE TO INITIALLY CHECK THE CODE IN. TO BE REPLACED BY C++ HTTP CLIENTS.
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

std::string YesThisIsCurl(const std::string& cmdline) {
  FILE* pipe = ::popen(cmdline.c_str(), "r");
  assert(pipe);
  char s[1024];
  ::fgets(s, sizeof(s), pipe);
  ::pclose(pipe);
  return s;
}

TEST(Net, HTTPCurlGET) {
  std::thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("GET", c.Method());
    EXPECT_EQ("/unittest", c.URL());
    c.SendHTTPResponse("PASSED");
  });
  EXPECT_EQ("PASSED",
            YesThisIsCurl(std::string("curl -s localhost:") + std::to_string(FLAGS_port) + "/unittest"));
  t.join();
}

TEST(Net, HTTPCurlPOST) {
  std::thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("POST", c.Method());
    EXPECT_EQ("/unittest_post", c.URL());
    EXPECT_EQ("BAZINGA", c.Body());
    c.SendHTTPResponse("POSTED");
  });
  EXPECT_EQ("POSTED",
            YesThisIsCurl(std::string("curl -s -d BAZINGA localhost:") + std::to_string(FLAGS_port) +
                          "/unittest_post"));
  t.join();
}

TEST(Net, HTTPCurlNoBodyPost) {
  std::thread t([]() {
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
            YesThisIsCurl(std::string("curl -s -X POST localhost:") + std::to_string(FLAGS_port) +
                          "/unittest_empty_post"));
  t.join();
}
