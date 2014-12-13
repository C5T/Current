#include <thread>

#include "http.h"

#include "../../dflags/dflags.h"

#include "../../3party/gtest/gtest.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

#include "../../strings/printf.h"

DEFINE_int32(port, 8080, "Local port to use for the test server.");

using std::string;
using std::thread;
using std::to_string;
using std::move;

using namespace bricks;

using bricks::net::Socket;
using bricks::net::ClientSocket;
using bricks::net::Connection;
using bricks::net::HTTPServerConnection;
using bricks::net::HTTPReceivedMessage;
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
        Syscall(strings::Printf("curl -s -X %s localhost:%d%s", method.c_str(), FLAGS_port, url.c_str()));
    server_thread.join();
    return result;
  }

  static string FetchWithBody(thread& server_thread,
                              const string& url,
                              const string& method,
                              const string& data) {
    const string result = Syscall(strings::Printf(
        "curl -s -X %s -d '%s' localhost:%d%s", method.c_str(), data.c_str(), FLAGS_port, url.c_str()));
    server_thread.join();
    return result;
  }
};

class HTTPClientImplPOSIX {
 public:
  static string Fetch(thread& server_thread, const string& url, const string& method) {
    return Impl(server_thread, url, method);
  }

  static string FetchWithBody(thread& server_thread,
                              const string& url,
                              const string& method,
                              const string& data) {
    return Impl(server_thread, url, method, true, data);
  }

 private:
  static string Impl(thread& server_thread,
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
    HTTPReceivedMessage message(connection);
    assert(message.HasBody());
    const string body = message.Body();
    server_thread.join();
    return body;
  }
};

template <typename T>
class HTTPTest : public ::testing::Test {};

typedef ::testing::Types<HTTPClientImplPOSIX, HTTPClientImplCURL> HTTPClientImplsTypeList;
TYPED_TEST_CASE(HTTPTest, HTTPClientImplsTypeList);

TYPED_TEST(HTTPTest, GET) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("GET", c.Message().Method());
             EXPECT_EQ("/unittest", c.Message().URL());
             c.SendHTTPResponse("PASSED");
           },
           Socket(FLAGS_port));
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, "/unittest", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/unittest_post", c.Message().URL());
             ASSERT_TRUE(c.Message().HasBody()) << "WTF!";
             EXPECT_EQ("BAZINGA", c.Message().Body());
             c.SendHTTPResponse("POSTED");
           },
           Socket(FLAGS_port));
  EXPECT_EQ("POSTED", TypeParam::FetchWithBody(t, "/unittest_post", "POST", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPOST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/unittest_empty_post", c.Message().URL());
             EXPECT_FALSE(c.Message().HasBody());
             ASSERT_THROW(c.Message().Body(), HTTPNoBodyProvidedException);
             c.SendHTTPResponse("ALMOST_POSTED");
           },
           Socket(FLAGS_port));
  EXPECT_EQ("ALMOST_POSTED", TypeParam::Fetch(t, "/unittest_empty_post", "POST"));
}
