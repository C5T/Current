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

struct Servers {
  struct Impl {
    Impl() : t1(&Impl::f1, this), t2(&Impl::f2, this), t3(&Impl::f3, this) {
    }

    ~Impl() {
      t1.join();
      t2.join();
      t3.join();
    }

    void f1() {
      Socket s(FLAGS_port);
      HTTPConnection c(s.Accept());
      ASSERT_TRUE(c);
      EXPECT_EQ("GET", c.Method());
      EXPECT_EQ("/unittest", c.URL());
      c.SendHTTPResponse("PASSED");
    }

    void f2() {
      Socket s(FLAGS_port + 1);
      HTTPConnection c(s.Accept());
      ASSERT_TRUE(c);
      EXPECT_EQ("POST", c.Method());
      EXPECT_EQ("/unittest_post", c.URL());
      EXPECT_EQ("BAZINGA", c.Body());
      c.SendHTTPResponse("POSTED");
    }

    void f3() {
      Socket s(FLAGS_port + 2);
      HTTPConnection c(s.Accept());
      ASSERT_TRUE(c);
      EXPECT_EQ("POST", c.Method());
      EXPECT_EQ("/unittest_empty_post", c.URL());
      EXPECT_FALSE(c.HasBody());
      ASSERT_THROW(c.Body(), HTTPNoBodyProvidedException);
      c.SendHTTPResponse("ALMOST_POSTED");
    }

    std::thread t1, t2, t3;
  };

  static void EnsureSpawned() {
    static Impl impl;
  }
};

std::string YesThisIsCurl(const std::string& cmdline) {
  FILE* pipe = ::popen(cmdline.c_str(), "r");
  assert(pipe);
  char s[1024];
  ::fgets(s, sizeof(s), pipe);
  ::pclose(pipe);
  return s;
}

TEST(Net, HTTPCurlGET) {
  Servers::EnsureSpawned();
  EXPECT_EQ("PASSED",
            YesThisIsCurl(std::string("curl -s localhost:") + std::to_string(FLAGS_port) + "/unittest"));
}

TEST(Net, HTTPCurlPOST) {
  Servers::EnsureSpawned();
  EXPECT_EQ("POSTED",
            YesThisIsCurl(std::string("curl -s -d BAZINGA localhost:") + std::to_string(FLAGS_port + 1) +
                          "/unittest_post"));
}

TEST(Net, HTTPCurlNoBodyPost) {
  Servers::EnsureSpawned();
  EXPECT_EQ("ALMOST_POSTED",
            YesThisIsCurl(std::string("curl -s -X POST localhost:") + std::to_string(FLAGS_port + 2) +
                          "/unittest_empty_post"));
}
