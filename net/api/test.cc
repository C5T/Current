/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#include "../../port.h"

#include "docu/client/docu_02httpclient_01_test.cc"
#include "docu/client/docu_02httpclient_02_test.cc"

#include "docu/server/docu_03httpserver_01_test.cc"
#include "docu/server/docu_03httpserver_02_test.cc"
#include "docu/server/docu_03httpserver_03_test.cc"
#include "docu/server/docu_03httpserver_04_test.cc"
#ifndef BRICKS_APPLE 
// Temporary disabled chunked-transfer test for Apple -- M.Z.
#include "docu/server/docu_03httpserver_05_test.cc"
#endif

#include <string>

#include "api.h"

#include "../url/url.h"

#include "../../dflags/dflags.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"
#include "../../strings/printf.h"
#include "../../util/singleton.h"
#include "../../file/file.h"
#include "../../cerealize/cerealize.h"

using std::string;

using bricks::strings::Printf;
using bricks::Singleton;
using bricks::FileSystem;
using bricks::FileException;

using bricks::cerealize::ParseJSON;

using bricks::net::Connection;
using bricks::net::HTTPResponseCodeValue;
using bricks::net::HTTPHeaders;

using bricks::net::DefaultInternalServerErrorMessage;
using bricks::net::DefaultFourOhFourMessage;
using bricks::net::DefaultMethodNotAllowedMessage;

using bricks::net::HTTPRedirectNotAllowedException;
using bricks::net::HTTPRedirectLoopException;
using bricks::net::SocketResolveAddressException;
using bricks::net::CannotServeStaticFilesOfUnknownMIMEType;

using namespace bricks::net::api;

DEFINE_int32(net_api_test_port,
             8082,
             "Local port to use for the test API-based HTTP server. NOTE: This port should be different from "
             "ports in other network-based tests, since API-driven HTTP server will hold it open for the whole "
             "lifetime of the binary.");
DEFINE_string(net_api_test_tmpdir, ".noshit", "Local path for the test to create temporary files in.");

struct HTTPAPITestObject {
  int number;
  std::string text;
  std::vector<int> array;  // Visual C++ does not support the `= { 1, 2, 3 };` non-static member initialization.
  HTTPAPITestObject() : number(42), text("text"), array({1, 2, 3}) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(number), CEREAL_NVP(text), CEREAL_NVP(array));
  }
};

#if !defined(BRICKS_COVERAGE_REPORT_MODE) && !defined(BRICKS_WINDOWS)
TEST(ArchitectureTest, BRICKS_ARCH_UNAME_AS_IDENTIFIER) {
  ASSERT_EQ(BRICKS_ARCH_UNAME, FLAGS_bricks_runtime_arch);
}
#endif

// Test the features of HTTP server.
TEST(HTTPAPI, Register) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/get", [](Request r) { r("OK"); });
  auto tmp_handler = [](Request) {};  // LCOV_EXCL_LINE
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/get", tmp_handler), HandlerAlreadyExistsException);
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/get", &tmp_handler), HandlerAlreadyExistsException);
  const string url = Printf("http://localhost:%d/get", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).HandlersCount());
}

TEST(HTTPAPI, UnRegister) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/foo", [](Request r) { r("bar"); });
  auto tmp_handler = [](Request) {};  // LCOV_EXCL_LINE
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/foo", tmp_handler), HandlerAlreadyExistsException);
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/foo", &tmp_handler), HandlerAlreadyExistsException);
  const string url = Printf("http://localhost:%d/foo", FLAGS_net_api_test_port);
  EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).HandlersCount());
  HTTP(FLAGS_net_api_test_port).UnRegister("/foo");
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(url)).code));
  EXPECT_EQ(0u, HTTP(FLAGS_net_api_test_port).HandlersCount());
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).UnRegister("/foo"), HandlerDoesNotExistException);
}

TEST(HTTPAPI, URLParameters) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/query", [](Request r) { r("x=" + r.url.query["x"]); });
  EXPECT_EQ("x=", HTTP(GET(Printf("http://localhost:%d/query", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("x=42", HTTP(GET(Printf("http://localhost:%d/query?x=42", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("x=test passed",
            HTTP(GET(Printf("http://localhost:%d/query?x=test+passed", FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, RespondsWithString) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/responds_with_string", [](Request r) {
    r("test_string", HTTPResponseCode.OK, "application/json", HTTPHeaders({{"foo", "bar"}}));
  });
  const string url = Printf("http://localhost:%d/responds_with_string", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("test_string", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).HandlersCount());
}

TEST(HTTPAPI, RespondsWithObject) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/responds_with_object", [](Request r) {
    r(HTTPAPITestObject(),
      "test_object",
      HTTPResponseCode.OK,
      "application/json",
      HTTPHeaders({{"foo", "bar"}}));
  });
  const string url = Printf("http://localhost:%d/responds_with_object", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"test_object\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).HandlersCount());
}

#ifndef BRICKS_APPLE
// Disabled redirect tests for Apple due to implementation specifics -- M.Z.
TEST(HTTPAPI, Redirect) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/from", [](Request r) {
    r("", HTTPResponseCode.Found, "text/html", HTTPHeaders({{"Location", "/to"}}));
  });
  HTTP(FLAGS_net_api_test_port).Register("/to", [](Request r) { r("Done."); });
  // Redirect not allowed by default.
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/from", FLAGS_net_api_test_port))),
               HTTPRedirectNotAllowedException);
  // Redirect allowed when `.AllowRedirects()` is set.
  const auto response = HTTP(GET(Printf("http://localhost:%d/from", FLAGS_net_api_test_port)).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Done.", response.body);
  EXPECT_EQ(Printf("http://localhost:%d/to", FLAGS_net_api_test_port), response.url);
}

TEST(HTTPAPI, RedirectLoop) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/p1", [](Request r) {
    r("", HTTPResponseCode.Found, "text/html", HTTPHeaders({{"Location", "/p2"}}));
  });
  HTTP(FLAGS_net_api_test_port).Register("/p2", [](Request r) {
    r("", HTTPResponseCode.Found, "text/html", HTTPHeaders({{"Location", "/p3"}}));
  });
  HTTP(FLAGS_net_api_test_port).Register("/p3", [](Request r) {
    r("", HTTPResponseCode.Found, "text/html", HTTPHeaders({{"Location", "/p1"}}));
  });
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/p1", FLAGS_net_api_test_port))), HTTPRedirectLoopException);
}
#endif

TEST(HTTPAPI, FourOhFour) {
  EXPECT_EQ("<h1>NOT FOUND</h1>\n", DefaultFourOhFourMessage());
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  const string url = Printf("http://localhost:%d/ORLY", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(404, static_cast<int>(response.code));
  EXPECT_EQ(DefaultFourOhFourMessage(), response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, HandlerByValuePerformsACopy) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  struct Helper {
    size_t counter = 0u;
    void operator()(Request r) {
      ++counter;
      r("Incremented.");
    }
  };
  Helper helper;
  EXPECT_EQ(0u, helper.counter);
  HTTP(FLAGS_net_api_test_port).Register("/incr", helper);
  EXPECT_EQ("Incremented.", HTTP(GET(Printf("http://localhost:%d/incr", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(0u, helper.counter);
}

TEST(HTTPAPI, HandlerByPointerPreservesObject) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  struct Helper {
    size_t counter = 0u;
    void operator()(Request r) {
      ++counter;
      r("Incremented two.");
    }
  };
  Helper helper;
  Helper copy(helper);
  EXPECT_EQ(0u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  HTTP(FLAGS_net_api_test_port).Register("/incr", &helper);
  HTTP(FLAGS_net_api_test_port).Register("/incr_same", &helper);
  HTTP(FLAGS_net_api_test_port).Register("/incr_copy", &copy);
  EXPECT_EQ("Incremented two.", HTTP(GET(Printf("http://localhost:%d/incr", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(1u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  EXPECT_EQ("Incremented two.", HTTP(GET(Printf("http://localhost:%d/incr_same", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(2u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  EXPECT_EQ("Incremented two.", HTTP(GET(Printf("http://localhost:%d/incr_copy", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(2u, helper.counter);
  EXPECT_EQ(1u, copy.counter);
}

TEST(HTTPAPI, HandlerSupportsStaticMethodsBothWays) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  struct Static {
    static void Foo(Request r) { r("foo"); }
    static void Bar(Request r) { r("bar"); }
  };
  HTTP(FLAGS_net_api_test_port).Register("/foo", Static::Foo);
  HTTP(FLAGS_net_api_test_port).Register("/bar", Static::Bar);
  HTTP(FLAGS_net_api_test_port).Register("/fooptr", &Static::Foo);
  HTTP(FLAGS_net_api_test_port).Register("/barptr", &Static::Bar);
  EXPECT_EQ("foo", HTTP(GET(Printf("http://localhost:%d/foo", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("bar", HTTP(GET(Printf("http://localhost:%d/bar", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("foo", HTTP(GET(Printf("http://localhost:%d/fooptr", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("bar", HTTP(GET(Printf("http://localhost:%d/barptr", FLAGS_net_api_test_port))).body);
}

// Don't wait 10 x 10ms beyond the 1st run when running tests in a loop.
struct ShouldReduceDelayBetweenChunksSingleton {
  bool yes = false;
};
// Test various HTTP client modes.
// Temporary disabled for Apple (not supporting chunk-transfer) -- M.Z.
#ifndef BRICKS_APPLE
TEST(HTTPAPI, GetToFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/stars", [](Request r) {
    const size_t n = atoi(r.url.query["n"].c_str());
    auto response = r.connection.SendChunkedHTTPResponse();
    const auto sleep = []() {
      const uint64_t delay_between_chunks = []() {
        bool& reduce = Singleton<ShouldReduceDelayBetweenChunksSingleton>().yes;
        if (!reduce) {
          reduce = true;
          return 10;
        } else {
          return 1;
        }
      }();
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_between_chunks));
    };
    sleep();
    for (size_t i = 0; i < n; ++i) {
      response.Send("*");
      sleep();
      response.Send(std::vector<char>({'a', 'b'}));
      sleep();
      response.Send(std::vector<uint8_t>({0x31, 0x32}));
      sleep();
    }
  });
  bricks::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_test_file_for_http_get";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/stars?n=3", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(file_name, response.body_file_name);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ("*ab12*ab12*ab12", FileSystem::ReadFileAsString(response.body_file_name));
}
#endif

TEST(HTTPAPI, PostFromBufferToBuffer) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r("Data: " + r.body);
  });
  const auto response =
      HTTP(POST(Printf("http://localhost:%d/post", FLAGS_net_api_test_port), "No shit!", "application/octet-stream"));
  EXPECT_EQ("Data: No shit!", response.body);
}

TEST(HTTPAPI, PostWithNoBodyProvided) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_FALSE(r.has_body);
    r("POST with no body passed in.");
  });
  EXPECT_EQ("POST with no body passed in.", HTTP(POST(Printf("http://localhost:%d/post", FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, PostAStringAsString) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post_string", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r(r.body);
  });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/post_string", FLAGS_net_api_test_port),
                      std::string("std::string"),
                      "text/plain")).body);
}

TEST(HTTPAPI, PostAStringAsConstCharPtr) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post_const_char_ptr", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r(r.body);
  });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/post_const_char_ptr", FLAGS_net_api_test_port),
                      static_cast<const char*>("const char*"),
                      "text/plain")).body);
}

TEST(HTTPAPI, RespondWithStringAsString) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/respond_with_std_string", [](Request r) {
    ASSERT_FALSE(r.has_body);
    r.connection.SendHTTPResponse(std::string("std::string"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/respond_with_std_string", FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, RespondWithStringAsConstCharPtr) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/respond_with_const_char_ptr", [](Request r) {
    ASSERT_FALSE(r.has_body);
    r.connection.SendHTTPResponse(static_cast<const char*>("const char*"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/respond_with_const_char_ptr", FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, RespondWithStringAsStringViaRequestDirectly) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/respond_with_std_string_via_request_directly", [](Request r) {
    ASSERT_FALSE(r.has_body);
    r(std::string("std::string"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/respond_with_std_string_via_request_directly",
                             FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, RespondWithStringAsConstCharPtrViaRequestDirectly) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/respond_with_const_char_ptr_via_request_directly", [](Request r) {
    ASSERT_FALSE(r.has_body);
    r(static_cast<const char*>("const char*"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/respond_with_const_char_ptr_via_request_directly",
                             FLAGS_net_api_test_port))).body);
}

struct ObjectToPOST {
  int x = 42;
  std::string s = "foo";
  std::string AsString() const { return Printf("%d:%s", x, s.c_str()); }
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(x), CEREAL_NVP(s));
  }
};

TEST(HTTPAPI, PostCerealizableObject) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r("Data: " + r.body);
  });
  EXPECT_EQ("Data: {\"data\":{\"x\":42,\"s\":\"foo\"}}",
            HTTP(POST(Printf("http://localhost:%d/post", FLAGS_net_api_test_port), ObjectToPOST())).body);
}

TEST(HTTPAPI, PostCerealizableObjectAndParseJSON) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r("Data: " + ParseJSON<ObjectToPOST>(r.body).AsString());
  });
  EXPECT_EQ("Data: 42:foo",
            HTTP(POST(Printf("http://localhost:%d/post", FLAGS_net_api_test_port), ObjectToPOST())).body);
}

TEST(HTTPAPI, PostCerealizableObjectAndFailToParseJSON) {
  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", DefaultInternalServerErrorMessage());
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    try {
      r("Data: " + ParseJSON<ObjectToPOST>(r.body).AsString());
    } catch (const std::exception&) {
      // Do nothing. "INTERNAL SERVER ERROR" should get returned by the framework.
    }
  });
  EXPECT_EQ(DefaultInternalServerErrorMessage(),
            HTTP(POST(Printf("http://localhost:%d/post", FLAGS_net_api_test_port), "fffuuuuu", "text/plain")).body);
}

#ifndef BRICKS_APPLE
// Disabled for Apple - native code doesn't throw exceptions -- M.Z.
TEST(HTTPAPI, PostFromInvalidFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  bricks::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string non_existent_file_name = FLAGS_net_api_test_tmpdir + "/non_existent_file";
  const auto test_file_scope = FileSystem::ScopedRmFile(non_existent_file_name);
  ASSERT_THROW(HTTP(POSTFromFile(
                   Printf("http://localhost:%d/foo", FLAGS_net_api_test_port), non_existent_file_name, "text/plain")),
               FileException);
}
#endif

TEST(HTTPAPI, PostFromFileToBuffer) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r("Voila: " + r.body);
  });
  bricks::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_input_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/post", FLAGS_net_api_test_port);
  FileSystem::WriteStringToFile("No shit detected.", file_name.c_str());
  const auto response = HTTP(POSTFromFile(url, file_name, "application/octet-stream"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Voila: No shit detected.", response.body);
}

TEST(HTTPAPI, PostFromBufferToFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r("Meh: " + r.body);
  });
  bricks::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_output_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/post", FLAGS_net_api_test_port);
  const auto response = HTTP(POST(url, "TEST BODY", "text/plain"), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Meh: TEST BODY", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, PostFromFileToFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request r) {
    ASSERT_TRUE(r.has_body);
    r("Phew: " + r.body);
  });
  bricks::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string request_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_request_test_file_for_http_post";
  const string response_file_name =
      FLAGS_net_api_test_tmpdir + "/some_complex_response_test_file_for_http_post";
  const auto input_file_scope = FileSystem::ScopedRmFile(request_file_name);
  const auto output_file_scope = FileSystem::ScopedRmFile(response_file_name);
  const string url = Printf("http://localhost:%d/post", FLAGS_net_api_test_port);
  const string post_body = "Aloha, this text should pass from one file to another. Mahalo!";
  FileSystem::WriteStringToFile(post_body, request_file_name.c_str());
  const auto response =
      HTTP(POSTFromFile(url, request_file_name, "text/plain"), SaveResponseToFile(response_file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Phew: Aloha, this text should pass from one file to another. Mahalo!",
            FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, UserAgent) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port)
      .Register("/ua", [](Request r) { r("TODO(dkorolev): Actually get passed in user agent."); });
  const string url = Printf("http://localhost:%d/ua", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url).UserAgent("Aloha"));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("TODO(dkorolev): Actually get passed in user agent.", response.body);
}

// Don't run this ~1s test more than once in a loop.
struct OnlyCheckForInvalidURLOnceSingleton {
  bool done = false;
};

#ifndef BRICKS_APPLE
TEST(HTTPAPI, InvalidUrl) {
  bool& done = Singleton<OnlyCheckForInvalidURLOnceSingleton>().done;
  if (!done) {
    ASSERT_THROW(HTTP(GET("http://999.999.999.999/")), SocketResolveAddressException);
    done = true;
  }
}
#endif

TEST(HTTPAPI, ServeDir) {
  EXPECT_EQ("<h1>METHOD NOT ALLOWED</h1>\n", DefaultMethodNotAllowedMessage());
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FLAGS_net_api_test_tmpdir + "/static";
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML</h1>", FileSystem::JoinPath(dir, "file.html").c_str());
  FileSystem::WriteStringToFile("This is text.", FileSystem::JoinPath(dir, "file.txt").c_str());
  FileSystem::WriteStringToFile("And this: PNG", FileSystem::JoinPath(dir, "file.png").c_str());
  FileSystem::MkDir(FileSystem::JoinPath(dir, "subdir_to_ignore"), FileSystem::MkDirParameters::Silent);
  HTTP(FLAGS_net_api_test_port).ServeStaticFilesFrom(dir);
  EXPECT_EQ("<h1>HTML</h1>", HTTP(GET(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("This is text.", HTTP(GET(Printf("http://localhost:%d/file.txt", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("And this: PNG", HTTP(GET(Printf("http://localhost:%d/file.png", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("<h1>NOT FOUND</h1>\n",
            HTTP(GET(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port))).body);
#ifndef BRICKS_APPLE
// Temporary disabled - post with no body is not supported -- M.Z.  
  EXPECT_EQ("<h1>METHOD NOT ALLOWED</h1>\n",
            HTTP(POST(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("<h1>NOT FOUND</h1>\n",
            HTTP(POST(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port))).body);
#endif
  EXPECT_EQ(200, static_cast<int>(HTTP(GET(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port))).code));
  EXPECT_EQ(404,
            static_cast<int>(HTTP(GET(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port))).code));
#ifndef BRICKS_APPLE
// Temporary disabled - post with no body is not supported -- M.Z.  
  EXPECT_EQ(405, static_cast<int>(HTTP(POST(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port))).code));
  EXPECT_EQ(
      404, static_cast<int>(HTTP(POST(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port))).code));
#endif 
  FileSystem::RmDir(FileSystem::JoinPath(dir, "subdir_to_ignore"), FileSystem::RmDirParameters::Silent);
  FileSystem::RmDir(dir, FileSystem::RmDirParameters::Silent);
}

TEST(HTTPAPI, ServeDirOnlyServesFilesOfKnownMIMEType) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FLAGS_net_api_test_tmpdir + "/wrong_static_files";
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("TXT is okay.", FileSystem::JoinPath(dir, "file.txt").c_str());
  FileSystem::WriteStringToFile("FOO is not! ", FileSystem::JoinPath(dir, "file.foo").c_str());
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).ServeStaticFilesFrom(dir),
               CannotServeStaticFilesOfUnknownMIMEType);
  FileSystem::RmDir(dir, FileSystem::RmDirParameters::Silent);
}
