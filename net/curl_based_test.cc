// Linux-specific unit tests for HTTP. Using external utilities.
//
// ONLY HERE TO INITIALLY CHECK THE CODE IN. TO BE REPLACED BY C++ HTTP CLIENTS.

// Don't write unit tests like this.
// Please, never write unit tests like this.
// Every time you write a test like this God kills a kitten.
// Hear me. Don't. Just don't. It's not worth your karma. Go do something else.

#include <thread>
#include <chrono>

#include "../dflags/dflags.h"

#include "posix_http_server.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

DEFINE_int32(port, 8081, "Port to use for the test.");

TEST(Net, HTTPCurlGET) {
  std::thread server_thread([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("GET", c.Method());
    EXPECT_EQ("/unittest", c.URL());
    c.SendHTTPResponse("PASSED");
  });

  FILE* f =
      ::popen((std::string("curl -s localhost:") + std::to_string(FLAGS_port) + "/unittest").c_str(), "r");
  ASSERT_TRUE(f);
  char s[1024];
  fgets(s, sizeof(s), f);
  EXPECT_EQ("PASSED", std::string(s));
  fclose(f);

  server_thread.join();
}

TEST(Net, HTTPCurlPOST) {
  std::thread server_thread([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("POST", c.Method());
    EXPECT_EQ("/unittest_post", c.URL());
    EXPECT_EQ("BAZINGA", c.Body());
    c.SendHTTPResponse("POSTED");
  });

  FILE* f = ::popen(
      (std::string("curl -s -d BAZINGA localhost:") + std::to_string(FLAGS_port) + "/unittest_post").c_str(),
      "r");
  ASSERT_TRUE(f);
  char s[1024];
  fgets(s, sizeof(s), f);
  EXPECT_EQ("POSTED", std::string(s));
  fclose(f);

  server_thread.join();
}

TEST(Net, HTTPCurlNoBodyPost) {
  std::thread server_thread([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("POST", c.Method());
    EXPECT_EQ("/unittest_empty_post", c.URL());
    EXPECT_FALSE(c.HasBody());
    ASSERT_THROW(c.Body(), HTTPNoBodyProvidedException);
    c.SendHTTPResponse("ALMOST_POSTED");
  });

  FILE* f = ::popen(
      (std::string("curl -s -X POST localhost:") + std::to_string(FLAGS_port) + "/unittest_empty_post").c_str(),
      "r");
  ASSERT_TRUE(f);
  char s[1024];
  fgets(s, sizeof(s), f);
  EXPECT_EQ("ALMOST_POSTED", std::string(s));
  fclose(f);

  server_thread.join();
}
