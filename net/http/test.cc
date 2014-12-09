#include <thread>

#include "posix.h"

#include "../../dflags/dflags.h"

#include "../../3party/gtest/gtest.h"
#include "../../3party/gtest/gtest-main.h"

#include "../../string/printf.h"
using namespace bricks::string;

DEFINE_int32(port, 8080, "Local port to use for the test server.");

using std::string;
using std::thread;
using std::to_string;
using std::move;

using bricks::net::Socket;
using bricks::net::ClientSocket;
using bricks::net::Connection;
using bricks::net::HTTPConnection;
using bricks::net::HTTPHeaderParser;
using bricks::net::HTTPNoBodyProvidedException;

struct HTTPClientImplCURL {
  static string Syscall(const string& cmdline) {
    FILE* pipe = ::popen(cmdline.c_str(), "r");
    assert(pipe);
    char s[1024];
    ::fgets(s, sizeof(s), pipe);
    ::pclose(pipe);
    return s;
  }

  static string Fetch(thread& server_thread, const string& url, const string& method) {
    const string result =
        Syscall(Printf("curl -s -X %s localhost:%d%s", method.c_str(), FLAGS_port, url.c_str()));
    server_thread.join();
    return result;
  }

  static string FetchWithBody(thread& server_thread,
                              const string& url,
                              const string& method,
                              const string& data) {
    const string result = Syscall(
        Printf("curl -s -X %s -d '%s' localhost:%d%s", method.c_str(), data.c_str(), FLAGS_port, url.c_str()));
    server_thread.join();
    return result;
  }
};

struct HTTPClientImplPOSIX : HTTPHeaderParser {
  struct Impl : HTTPHeaderParser {
    Impl(thread& server_thread,
         const string& url,
         const string& method,
         bool has_data = false,
         const string& data = "") {
      Connection connection(ClientSocket("localhost", FLAGS_port));
      connection.BlockingWrite(method + ' ' + url + "\r\n");
      if (has_data) {
        connection.BlockingWrite("Content-Length: " + to_string(data.length()) + "\r\n");
      }
      connection.BlockingWrite("\r\n");
      if (has_data) {
        connection.BlockingWrite(data);
      }
      connection.SendEOF();
      HTTPHeaderParser::ParseHTTPHeader(connection);
      server_thread.join();
    }
  };

  static string Fetch(thread& server_thread, const string& url, const string& method) {
    return Impl(server_thread, url, method).Body();
  }

  static string FetchWithBody(thread& server_thread,
                              const string& url,
                              const string& method,
                              const string& data) {
    return Impl(server_thread, url, method, true, data).Body();
  }
};

template <typename T>
class HTTPTest : public ::testing::Test {};

typedef ::testing::Types<HTTPClientImplPOSIX, HTTPClientImplCURL> HTTPClientImplsTypeList;
TYPED_TEST_CASE(HTTPTest, HTTPClientImplsTypeList);

TYPED_TEST(HTTPTest, GET) {
  thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("GET", c.Method());
    EXPECT_EQ("/unittest", c.URL());
    c.SendHTTPResponse("PASSED");
  });
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, "/unittest", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  thread t([]() {
    Socket s(FLAGS_port);
    HTTPConnection c(s.Accept());
    ASSERT_TRUE(c);
    EXPECT_EQ("POST", c.Method());
    EXPECT_EQ("/unittest_post", c.URL());
    EXPECT_EQ("BAZINGA", c.Body());
    c.SendHTTPResponse("POSTED");
  });
  EXPECT_EQ("POSTED", TypeParam::FetchWithBody(t, "/unittest_post", "POST", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPost) {
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
  EXPECT_EQ("ALMOST_POSTED", TypeParam::Fetch(t, "/unittest_empty_post", "POST"));
}
