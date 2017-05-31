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
#include "docu/server/docu_03httpserver_05_test.cc"

#include <string>

#include "api.h"

#include "../URL/url.h"

#include "../../TypeSystem/struct.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/strings/join.h"
#include "../../Bricks/strings/printf.h"
#include "../../Bricks/util/singleton.h"
#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

using std::string;

using current::strings::Printf;
using current::Singleton;
using current::FileSystem;
using current::FileException;

using current::net::Connection;
using current::net::HTTPResponseCodeValue;
using current::net::http::Headers;

using current::net::DefaultInternalServerErrorMessage;
using current::net::DefaultFourOhFourMessage;
using current::net::DefaultMethodNotAllowedMessage;

using current::net::HTTPRedirectNotAllowedException;
using current::net::HTTPRedirectLoopException;
using current::net::SocketResolveAddressException;
using current::net::CannotServeStaticFilesOfUnknownMIMEType;

using namespace current::http;

DEFINE_int32(net_api_test_port,
             PickPortForUnitTest(),
             "Local port to use for the test API-based HTTP server. NOTE: This port should be different from "
             "ports in other network-based tests, since API-driven HTTP server will hold it open for the whole "
             "lifetime of the binary.");
DEFINE_string(net_api_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

CURRENT_STRUCT(HTTPAPITestObject) {
  CURRENT_FIELD(number, int32_t);
  CURRENT_FIELD(text, std::string);
  CURRENT_FIELD(array, std::vector<int>);
  CURRENT_DEFAULT_CONSTRUCTOR(HTTPAPITestObject) : number(42), text("text"), array({1, 2, 3}) {}
};

#if !defined(CURRENT_COVERAGE_REPORT_MODE) && !defined(CURRENT_WINDOWS)
TEST(ArchitectureTest, CURRENT_ARCH_UNAME_AS_IDENTIFIER) { ASSERT_EQ(CURRENT_ARCH_UNAME, FLAGS_current_runtime_arch); }
#endif

// Test the features of HTTP server.
TEST(HTTPAPI, Register) {
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/get", [](Request r) { r("OK"); });
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/get", nullptr), HandlerAlreadyExistsException);
  const string url = Printf("http://localhost:%d/get", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
}

TEST(HTTPAPI, RegisterExceptions) {
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("no_slash", nullptr), PathDoesNotStartWithSlash);
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/wrong_slash/", nullptr), PathEndsWithSlash);
  // The curly brackets are not necessarily wrong, but `URL::IsPathValidToRegister()` is `false` for them.
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/{}", nullptr), PathContainsInvalidCharacters);
}

TEST(HTTPAPI, RegisterWithURLPathParams) {
  const auto handler = [](Request r) {
                         r(r.url.path + " (" + current::strings::Join(r.url_path_args, ", ") + ") " + r.url_original.path);
                       };

  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/", URLPathArgs::CountMask::Any, handler) +
                     HTTP(FLAGS_net_api_test_port)
                         .Register("/user", URLPathArgs::CountMask::One | URLPathArgs::CountMask::Two, handler) +
                     HTTP(FLAGS_net_api_test_port).Register("/user/a", URLPathArgs::CountMask::One, handler) +
                     HTTP(FLAGS_net_api_test_port).Register("/user/a/1", URLPathArgs::CountMask::None, handler);

  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/", handler), HandlerAlreadyExistsException);
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/user", URLPathArgs::CountMask::Two, handler),
               HandlerAlreadyExistsException);
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/user/a", URLPathArgs::CountMask::One, handler),
               HandlerAlreadyExistsException);
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/user/a/1", handler), HandlerAlreadyExistsException);

  const auto run = [](const std::string& path) -> std::string {
    return HTTP(GET(Printf("http://localhost:%d", FLAGS_net_api_test_port) + path)).body;
  };

  EXPECT_EQ("/ () /", run("/"));
  EXPECT_EQ("/ (foo) /foo", run("/foo"));
  EXPECT_EQ("/ (foo) /foo/", run("/foo/"));
  EXPECT_EQ("/ (foo, bar) /foo/bar", run("/foo/bar"));
  EXPECT_EQ("/ (foo, bar) /foo/bar/", run("/foo/bar/"));
  EXPECT_EQ("/ (user) /user", run("/user"));
  EXPECT_EQ("/ (user) /user/", run("/user/"));

  EXPECT_EQ("/ () //", run("//"));
  EXPECT_EQ("/ () ///", run("///"));

  EXPECT_EQ("/user (a) /user/a", run("/user/a"));
  EXPECT_EQ("/user (a) /user/a/", run("/user/a/"));
  EXPECT_EQ("/user (a) /user///a", run("/user///a"));
  EXPECT_EQ("/user (a) /user///a///", run("/user///a///"));

  EXPECT_EQ("/user (x, y) /user/x/y", run("/user/x/y"));
  EXPECT_EQ("/user (x, y) /user/x/y/", run("/user/x/y/"));
  EXPECT_EQ("/user (x, y) /user///x//y//", run("/user///x//y//"));
  EXPECT_EQ("/user (x, y) /user///x//y//", run("/user///x//y//"));

  EXPECT_EQ("/user/a (0) /user/a/0", run("/user/a/0"));
  EXPECT_EQ("/user/a (0) /user/a/0/", run("/user/a/0/"));

  EXPECT_EQ("/user/a/1 () /user/a/1", run("/user/a/1"));
  EXPECT_EQ("/user/a/1 () /user/a/1/", run("/user/a/1/"));

  EXPECT_EQ("/user/a (2) /user/a/2", run("/user/a/2"));
  EXPECT_EQ("/user/a (2) /user/a/2/", run("/user/a/2/"));

  EXPECT_EQ("/ (user, a, 1, blah) /user/a/1/blah", run("/user/a/1/blah"));
  EXPECT_EQ("/ (user, a, 1, blah) /user/a/1/blah/", run("/user/a/1/blah/"));
}

TEST(HTTPAPI, ScopeLeftHangingThrowsAnException) {
  const string url = Printf("http://localhost:%d/foo", FLAGS_net_api_test_port);

  HTTP(FLAGS_net_api_test_port).Register("/foo", [](Request r) { r("bar"); });
  // DIMA
  // ASSERT_THROW(HTTP(FLAGS_net_api_test_port).UnRegister("/foo"), HandlerDoesNotExistException);
}

TEST(HTTPAPI, ScopedUnRegister) {
  const string url = Printf("http://localhost:%d/foo", FLAGS_net_api_test_port);

  {
    EXPECT_EQ(0u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
    HTTPRoutesScope registerer = HTTP(FLAGS_net_api_test_port).Register("/foo", [](Request r) { r("bar"); });
    EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
    EXPECT_EQ("bar", HTTP(GET(url)).body);
  }

  {
    EXPECT_EQ(0u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
    HTTPRoutesScope registerer;
    registerer += HTTP(FLAGS_net_api_test_port).Register("/foo", [](Request r) { r("baz"); });
    EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
    EXPECT_EQ("baz", HTTP(GET(url)).body);
  }

  {
    EXPECT_EQ(0u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(url)).code));
  }
}

TEST(HTTPAPI, ScopeCanBeAssignedNullPtr) {
  auto scope = HTTP(FLAGS_net_api_test_port).Register("/are_we_there_yet", [](Request r) { r("So far."); });
  const string url = Printf("http://localhost:%d/are_we_there_yet", FLAGS_net_api_test_port);
  EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
  scope = nullptr;
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(url)).code));
}

TEST(HTTPAPI, URLParameters) {
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/query", [](Request r) { r("x=" + r.url.query["x"]); });
  EXPECT_EQ("x=", HTTP(GET(Printf("http://localhost:%d/query", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("x=42", HTTP(GET(Printf("http://localhost:%d/query?x=42", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("x=test passed",
            HTTP(GET(Printf("http://localhost:%d/query?x=test+passed", FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, InvalidHEXInURLParameters) {
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/qod", [](Request r) { r("wtf=" + r.url.query["wtf"]); });
  {
    const auto ok1 = HTTP(GET(Printf("http://localhost:%d/qod?wtf=OK", FLAGS_net_api_test_port)));
    EXPECT_EQ("wtf=OK", ok1.body);
    EXPECT_EQ(200, static_cast<int>(ok1.code));
  }
  {
    // Hexadecimal `4F4B` is 'OK'. Test both uppercase and lowercase.
    const auto ok2 = HTTP(GET(Printf("http://localhost:%d/qod?wtf=%%4F%%4b", FLAGS_net_api_test_port)));
    EXPECT_EQ("wtf=OK", ok2.body);
    EXPECT_EQ(200, static_cast<int>(ok2.code));
  }
  {
    // The presence of `%OK`, which is obviously wrong HEX code, in the URL should not kill the server.
    const auto ok3 = HTTP(GET(Printf("http://localhost:%d/qod?wtf=%%OK", FLAGS_net_api_test_port)));
    EXPECT_EQ(Printf("wtf=%%OK"), ok3.body);
    EXPECT_EQ(200, static_cast<int>(ok3.code));
  }
}

TEST(HTTPAPI, HeadersAndCookies) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/headers_and_cookies",
                                   [](Request r) {
                                     EXPECT_TRUE(r.headers.Has("Header1"));
                                     EXPECT_TRUE(r.headers.Has("Header2"));
                                     EXPECT_EQ("foo", r.headers["Header1"].value);
                                     EXPECT_EQ("bar", r.headers["Header2"].value);
                                     EXPECT_EQ("x=1; y=2", r.headers.CookiesAsString());
                                     Response response("OK");
                                     response.headers.Set("X-Current-H1", "header1");
                                     response.SetCookie("cookie1", "value1");
                                     response.headers.Set("X-Current-H2", "header2");
                                     response.SetCookie("cookie2", "value2");
                                     r(response);
                                   });
  const auto response = HTTP(GET(Printf("http://localhost:%d/headers_and_cookies", FLAGS_net_api_test_port))
                                 .SetHeader("Header1", "foo")
                                 .SetCookie("x", "1")
                                 .SetCookie("y", "2")
                                 .SetHeader("Header2", "bar"));
  EXPECT_EQ("OK", response.body);
  EXPECT_TRUE(response.headers.Has("X-Current-H1"));
  EXPECT_TRUE(response.headers.Has("X-Current-H2"));
  EXPECT_EQ("header1", response.headers.Get("X-Current-H1"));
  EXPECT_EQ("header2", response.headers.Get("X-Current-H2"));
  EXPECT_EQ("cookie1=value1; cookie2=value2", response.headers.CookiesAsString());
}

TEST(HTTPAPI, ConnectionIPAndPort) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/foo",
                                   [](Request r) {
                                     const auto& c = r.connection;
                                     EXPECT_EQ("127.0.0.1", c.LocalIPAndPort().ip);
                                     EXPECT_EQ(FLAGS_net_api_test_port, c.LocalIPAndPort().port);
                                     EXPECT_EQ("127.0.0.1", c.RemoteIPAndPort().ip);
                                     EXPECT_LT(0, c.RemoteIPAndPort().port);
                                     r("bar", HTTPResponseCode.OK);
                                   });
  const string url = Printf("http://localhost:%d/foo", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("bar", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
}

TEST(HTTPAPI, RespondsWithString) {
  const auto scope =
      HTTP(FLAGS_net_api_test_port)
          .Register("/responds_with_string",
                    [](Request r) {
                      r("test_string", HTTPResponseCode.OK, "application/json", Headers({{"foo", "bar"}}));
                    });
  const string url = Printf("http://localhost:%d/responds_with_string", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("bar", response.headers.Get("foo"));
  EXPECT_EQ("test_string", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
}

TEST(HTTPAPI, RespondsWithObject) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/responds_with_object",
                                   [](Request r) {
                                     r(HTTPAPITestObject(),
                                       "test_object",
                                       HTTPResponseCode.OK,
                                       "application/json",
                                       Headers({{"foo", "bar"}}));
                                   });
  const string url = Printf("http://localhost:%d/responds_with_object", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"test_object\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
}

struct GoodStuff {
  void RespondViaHTTP(Request r) const {
    r("Good stuff.", HTTPResponseCode(762));  // https://github.com/joho/7XX-rfc
  }
};

TEST(HTTPAPI, RespondsWithCustomObject) {
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/dude_this_is_awesome", [](Request r) { r(GoodStuff()); });
  const string url = Printf("http://localhost:%d/dude_this_is_awesome", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(762, static_cast<int>(response.code));
  EXPECT_EQ("Good stuff.", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).PathHandlersCount());
}

#if !defined(CURRENT_APPLE) || defined(CURRENT_APPLE_HTTP_CLIENT_POSIX)
// Disabled redirect tests for Apple due to implementation specifics -- M.Z.
TEST(HTTPAPI, Redirect) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/from",
                                   [](Request r) {
                                     r("",
                                       HTTPResponseCode.Found,
                                       current::net::constants::kDefaultHTMLContentType,
                                       Headers({{"Location", "/to"}}));
                                   }) +
                     HTTP(FLAGS_net_api_test_port).Register("/to", [](Request r) { r("Done."); });
  // Redirect not allowed by default.
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/from", FLAGS_net_api_test_port))), HTTPRedirectNotAllowedException);
  // Redirect allowed when `.AllowRedirects()` is set.
  const auto response = HTTP(GET(Printf("http://localhost:%d/from", FLAGS_net_api_test_port)).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Done.", response.body);
  EXPECT_EQ(Printf("http://localhost:%d/to", FLAGS_net_api_test_port), response.url);
}

TEST(HTTPAPI, RedirectLoop) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/p1",
                                   [](Request r) {
                                     r("",
                                       HTTPResponseCode.Found,
                                       current::net::constants::kDefaultHTMLContentType,
                                       Headers({{"Location", "/p2"}}));
                                   }) +
                     HTTP(FLAGS_net_api_test_port)
                         .Register("/p2",
                                   [](Request r) {
                                     r("",
                                       HTTPResponseCode.Found,
                                       current::net::constants::kDefaultHTMLContentType,
                                       Headers({{"Location", "/p3"}}));
                                   }) +
                     HTTP(FLAGS_net_api_test_port)
                         .Register("/p3",
                                   [](Request r) {
                                     r("",
                                       HTTPResponseCode.Found,
                                       current::net::constants::kDefaultHTMLContentType,
                                       Headers({{"Location", "/p1"}}));
                                   });
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/p1", FLAGS_net_api_test_port))), HTTPRedirectLoopException);
}
#endif

TEST(HTTPAPI, FourOhFour) {
  EXPECT_EQ("<h1>NOT FOUND</h1>\n", DefaultFourOhFourMessage());
  const string url = Printf("http://localhost:%d/ORLY", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(404, static_cast<int>(response.code));
  EXPECT_EQ(DefaultFourOhFourMessage(), response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, HandlerIsCapturedByReference) {
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
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/incr", helper) +
                     HTTP(FLAGS_net_api_test_port).Register("/incr_same", helper) +
                     HTTP(FLAGS_net_api_test_port).Register("/incr_copy", copy);
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

TEST(HTTPAPI, HandlerSupportsStaticMethods) {
  struct Static {
    static void Foo(Request r) { r("foo"); }
    static void Bar(Request r) { r("bar"); }
  };
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/foo", Static::Foo) +
                     HTTP(FLAGS_net_api_test_port).Register("/bar", Static::Bar);
  EXPECT_EQ("foo", HTTP(GET(Printf("http://localhost:%d/foo", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("bar", HTTP(GET(Printf("http://localhost:%d/bar", FLAGS_net_api_test_port))).body);
}

// Don't wait 10 x 10ms beyond the 1st run when running tests in a loop.
struct ShouldReduceDelayBetweenChunksSingleton {
  bool yes = false;
};
// Test various HTTP client modes.
TEST(HTTPAPI, GetToFile) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/stars",
                                   [](Request r) {
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
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_test_file_for_http_get";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/stars?n=3", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(file_name, response.body_file_name);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ("*ab12*ab12*ab12", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, ChunkedResponseWithHeaders) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/chunked_with_header",
                                   [](Request r) {
                                     EXPECT_EQ("GET", r.method);
                                     auto response = r.connection.SendChunkedHTTPResponse(
                                         HTTPResponseCode.OK, "text/plain", Headers({{"header", "yeah"}}));
                                     response.Send("A");
                                     response.Send("B");
                                     response.Send("C");
                                   });
  const auto response = HTTP(GET(Printf("http://localhost:%d/chunked_with_header", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("ABC", response.body);
  ASSERT_TRUE(response.headers.Has("header"));
  EXPECT_EQ("yeah", response.headers.Get("header"));
}

// A hacky way to get back the response chunk by chunk. TODO(dkorolev): `ChunkedGET`.
TEST(HTTPAPI, GetByChunksPrototype) {
  // Handler returning the result chunk by chunk.
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/chunks",
                                   [](Request r) {
                                     auto response = r.connection.SendChunkedHTTPResponse(
                                         HTTPResponseCode.OK, "text/plain", Headers({{"header", "oh-well"}}));
                                     response.Send("1\n");
                                     response.Send("23\n");
                                     response.Send("456\n");
                                   });
  const string url = Printf("http://localhost:%d/chunks", FLAGS_net_api_test_port);
  {
    // A conventional GET, ignoring chunk boundaries and concatenating all the data together.
    const auto response = HTTP(GET(url));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("1\n23\n456\n", response.body);
    ASSERT_TRUE(response.headers.Has("header"));
    EXPECT_EQ("oh-well", response.headers.Get("header"));
  }
  {
    // A slightly more internal version.
    HTTPClientPOSIX client((current::http::impl::HTTPRedirectHelper::ConstructionParams()));
    client.request_method_ = "GET";
    client.request_url_ = url;
    ASSERT_TRUE(client.Go());
    EXPECT_EQ("1\n23\n456\n", client.HTTPRequest().Body());
  }
  {
    // A prototype of fetching the result chunk by chunk.
    class ChunkByChunkHTTPResponseReceiver {
     public:
      struct ConstructionParams {
        std::function<void(const std::string&, const std::string&)> header_callback;
        std::function<void(const std::string&)> chunk_callback;
        std::function<void()> done_callback;

        ConstructionParams() = delete;

        ConstructionParams(std::function<void(const std::string&, const std::string&)> header_callback,
                           std::function<void(const std::string&)> chunk_callback,
                           std::function<void()> done_callback)
            : header_callback(header_callback), chunk_callback(chunk_callback), done_callback(done_callback) {}

        ConstructionParams(const ConstructionParams& rhs) = default;
      };

      ChunkByChunkHTTPResponseReceiver() = delete;
      ChunkByChunkHTTPResponseReceiver(const ConstructionParams& params) : params(params) {}

      const ConstructionParams params;

      const std::string location = "";  // To please `HTTPClientPOSIX`. -- D.K.
      const current::net::http::Headers& headers() const { return headers_; }

     protected:
      inline void OnHeader(const char* key, const char* value) { params.header_callback(key, value); }

      inline void OnChunk(const char* chunk, size_t length) { params.chunk_callback(std::string(chunk, length)); }

      inline void OnChunkedBodyDone(const char*& begin, const char*& end) {
        params.done_callback();
        begin = nullptr;
        end = nullptr;
      }

     private:
      current::net::http::Headers headers_;
    };

    std::vector<std::string> headers;
    std::vector<std::string> chunk_by_chunk_response;
    const auto header_callback =
        [&headers](const std::string& k, const std::string& v) { headers.push_back(k + '=' + v); };
    const auto chunk_callback =
        [&chunk_by_chunk_response](const std::string& s) { chunk_by_chunk_response.push_back(s); };
    const auto done_callback = [&chunk_by_chunk_response]() { chunk_by_chunk_response.push_back("DONE"); };

    current::http::GenericHTTPClientPOSIX<ChunkByChunkHTTPResponseReceiver> client(
        ChunkByChunkHTTPResponseReceiver::ConstructionParams(header_callback, chunk_callback, done_callback));
    client.request_method_ = "GET";
    client.request_url_ = url;
    ASSERT_TRUE(client.Go());
    EXPECT_EQ("1\n|23\n|456\n|DONE", current::strings::Join(chunk_by_chunk_response, '|'));
    EXPECT_EQ(4u, headers.size());
    EXPECT_EQ("Content-Type=text/plain Connection=keep-alive header=oh-well Transfer-Encoding=chunked",
              current::strings::Join(headers, ' '));
  }
  {
    // The full version of how continuous `ChunkedGET` functions.
    std::vector<std::string> headers;
    std::vector<std::string> chunk_by_chunk_response;

    const auto response =
        HTTP(ChunkedGET(url,
                        [&headers](const std::string& k, const std::string& v) { headers.push_back(k + '=' + v); },
                        [&chunk_by_chunk_response](const std::string& s) { chunk_by_chunk_response.push_back(s); },
                        [&chunk_by_chunk_response]() { chunk_by_chunk_response.push_back("DONE"); }));
    EXPECT_EQ(200, static_cast<int>(response));
    EXPECT_EQ("1\n|23\n|456\n|DONE", current::strings::Join(chunk_by_chunk_response, '|'));
    EXPECT_EQ(4u, headers.size());
    EXPECT_EQ("Content-Type=text/plain Connection=keep-alive header=oh-well Transfer-Encoding=chunked",
              current::strings::Join(headers, ' '));
  }
}

TEST(HTTPAPI, PostFromBufferToBuffer) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/post",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     r("Data: " + r.body);
                                   });
  const auto response =
      HTTP(POST(Printf("http://localhost:%d/post", FLAGS_net_api_test_port), "No shit!", "application/octet-stream"));
  EXPECT_EQ("Data: No shit!", response.body);
}

TEST(HTTPAPI, PostAStringAsString) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/post_string",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     EXPECT_EQ("POST", r.method);
                                     r(r.body);
                                   });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/post_string", FLAGS_net_api_test_port),
                      std::string("std::string"),
                      "text/plain")).body);
}

TEST(HTTPAPI, PostAStringAsConstCharPtr) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/post_const_char_ptr",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     EXPECT_EQ("POST", r.method);
                                     r(r.body);
                                   });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/post_const_char_ptr", FLAGS_net_api_test_port),
                      static_cast<const char*>("const char*"),
                      "text/plain")).body);
}

TEST(HTTPAPI, RespondWithStringAsString) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/respond_with_std_string",
                                   [](Request r) {
                                     EXPECT_EQ("POST", r.method);
                                     EXPECT_EQ("", r.body);
                                     r.connection.SendHTTPResponse(std::string("std::string"), HTTPResponseCode.OK);
                                   });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/respond_with_std_string", FLAGS_net_api_test_port), "")).body);
}

TEST(HTTPAPI, RespondWithStringAsConstCharPtr) {
  const auto scope =
      HTTP(FLAGS_net_api_test_port)
          .Register("/respond_with_const_char_ptr",
                    [](Request r) {
                      EXPECT_EQ("", r.body);
                      r.connection.SendHTTPResponse(static_cast<const char*>("const char*"), HTTPResponseCode.OK);
                    });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/respond_with_const_char_ptr", FLAGS_net_api_test_port), "")).body);
}

TEST(HTTPAPI, RespondWithStringAsStringViaRequestDirectly) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/respond_with_std_string_via_request_directly",
                                   [](Request r) {
                                     EXPECT_EQ("", r.body);
                                     r(std::string("std::string"), HTTPResponseCode.OK);
                                   });
  EXPECT_EQ(
      "std::string",
      HTTP(POST(Printf("http://localhost:%d/respond_with_std_string_via_request_directly", FLAGS_net_api_test_port),
                "")).body);
}

TEST(HTTPAPI, RespondWithStringAsConstCharPtrViaRequestDirectly) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/respond_with_const_char_ptr_via_request_directly",
                                   [](Request r) {
                                     EXPECT_EQ("", r.body);
                                     r(static_cast<const char*>("const char*"), HTTPResponseCode.OK);
                                   });
  EXPECT_EQ(
      "const char*",
      HTTP(POST(Printf("http://localhost:%d/respond_with_const_char_ptr_via_request_directly", FLAGS_net_api_test_port),
                "")).body);
}

CURRENT_STRUCT(SerializableObject) {
  CURRENT_FIELD(x, int, 42);
  CURRENT_FIELD(s, std::string, "foo");
  std::string AsString() const { return Printf("%d:%s", x, s.c_str()); }
};

#if !defined(CURRENT_APPLE) || defined(CURRENT_APPLE_HTTP_CLIENT_POSIX)
// Disabled for Apple - native code doesn't throw exceptions -- M.Z.
TEST(HTTPAPI, PostFromInvalidFile) {
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string non_existent_file_name = FLAGS_net_api_test_tmpdir + "/non_existent_file";
  const auto test_file_scope = FileSystem::ScopedRmFile(non_existent_file_name);
  ASSERT_THROW(HTTP(POSTFromFile(
                   Printf("http://localhost:%d/foo", FLAGS_net_api_test_port), non_existent_file_name, "text/plain")),
               FileException);
}
#endif

TEST(HTTPAPI, PostFromFileToBuffer) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/post",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     r("Voila: " + r.body);
                                   });
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_input_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/post", FLAGS_net_api_test_port);
  FileSystem::WriteStringToFile("No shit detected.", file_name.c_str());
  const auto response = HTTP(POSTFromFile(url, file_name, "application/octet-stream"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Voila: No shit detected.", response.body);
}

TEST(HTTPAPI, PostFromBufferToFile) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/post",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     r("Meh: " + r.body);
                                   });
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_output_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/post", FLAGS_net_api_test_port);
  const auto response = HTTP(POST(url, "TEST BODY", "text/plain"), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Meh: TEST BODY", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, PostFromFileToFile) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/post",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     r("Phew: " + r.body);
                                   });
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string request_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_request_test_file_for_http_post";
  const string response_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_response_test_file_for_http_post";
  const auto input_file_scope = FileSystem::ScopedRmFile(request_file_name);
  const auto output_file_scope = FileSystem::ScopedRmFile(response_file_name);
  const string url = Printf("http://localhost:%d/post", FLAGS_net_api_test_port);
  const string post_body = "Hi, this text should pass from one file to another. Mahalo!";
  FileSystem::WriteStringToFile(post_body, request_file_name.c_str());
  const auto response =
      HTTP(POSTFromFile(url, request_file_name, "text/plain"), SaveResponseToFile(response_file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Phew: Hi, this text should pass from one file to another. Mahalo!",
            FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, HeadRequest) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/head",
                                   [](Request r) {
                                     EXPECT_EQ("HEAD", r.method);
                                     ASSERT_TRUE(r.body.empty());
                                     r("", HTTPResponseCode.OK, "text/html", Headers({{"foo", "bar"}}));
                                   });
  const auto response = HTTP(HEAD(Printf("http://localhost:%d/head", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_TRUE(response.body.empty());
  ASSERT_TRUE(response.headers.Has("foo"));
  EXPECT_EQ("bar", response.headers.Get("foo"));
}

TEST(HTTPAPI, DeleteRequest) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/delete",
                                   [](Request r) {
                                     EXPECT_EQ("DELETE", r.method);
                                     ASSERT_TRUE(r.body.empty());
                                     SerializableObject object;
                                     r(object);
                                   });
  const auto response = HTTP(DELETE(Printf("http://localhost:%d/delete", FLAGS_net_api_test_port)));
  EXPECT_EQ("42:foo", ParseJSON<SerializableObject>(response.body).AsString());
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, PatchRequest) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/patch",
                                   [](Request r) {
                                     EXPECT_EQ("PATCH", r.method);
                                     EXPECT_EQ("test", r.body);
                                     r("Patch OK.");
                                   });
  const auto response = HTTP(PATCH(Printf("http://localhost:%d/patch", FLAGS_net_api_test_port), "test"));
  EXPECT_EQ("Patch OK.", response.body);
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, UserAgent) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/ua", [](Request r) { r("TODO(dkorolev): Actually get passed in user agent."); });
  const string url = Printf("http://localhost:%d/ua", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url).UserAgent("Blah"));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("TODO(dkorolev): Actually get passed in user agent.", response.body);
}

// Don't run this ~1s test more than once in a loop.
struct OnlyCheckForInvalidURLOnceSingleton {
  bool done = false;
};

#if !defined(CURRENT_APPLE) || defined(CURRENT_APPLE_HTTP_CLIENT_POSIX)
// Disabled for Apple - native code doesn't throw exceptions -- M.Z.
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
  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FLAGS_net_api_test_tmpdir + "/static";
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML</h1>", FileSystem::JoinPath(dir, "file.html").c_str());
  FileSystem::WriteStringToFile("This is text.", FileSystem::JoinPath(dir, "file.txt").c_str());
  FileSystem::WriteStringToFile("And this: PNG", FileSystem::JoinPath(dir, "file.png").c_str());
  FileSystem::MkDir(FileSystem::JoinPath(dir, "subdir_to_ignore"), FileSystem::MkDirParameters::Silent);
  const auto scope = HTTP(FLAGS_net_api_test_port).ServeStaticFilesFrom(dir);
  EXPECT_EQ("<h1>HTML</h1>", HTTP(GET(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("This is text.", HTTP(GET(Printf("http://localhost:%d/file.txt", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("And this: PNG", HTTP(GET(Printf("http://localhost:%d/file.png", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("<h1>NOT FOUND</h1>\n",
            HTTP(GET(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("<h1>METHOD NOT ALLOWED</h1>\n",
            HTTP(POST(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port), "")).body);
  EXPECT_EQ("<h1>NOT FOUND</h1>\n",
            HTTP(POST(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port), "")).body);
  EXPECT_EQ(200, static_cast<int>(HTTP(GET(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port))).code));
  EXPECT_EQ(404,
            static_cast<int>(HTTP(GET(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port))).code));
  // Temporary disabled - post with no body is not supported -- M.Z.
  EXPECT_EQ(405,
            static_cast<int>(HTTP(POST(Printf("http://localhost:%d/file.html", FLAGS_net_api_test_port), "")).code));
  EXPECT_EQ(
      404,
      static_cast<int>(HTTP(POST(Printf("http://localhost:%d/subdir_to_ignore", FLAGS_net_api_test_port), "")).code));
  FileSystem::RmDir(FileSystem::JoinPath(dir, "subdir_to_ignore"), FileSystem::RmDirParameters::Silent);
  FileSystem::RmDir(dir, FileSystem::RmDirParameters::Silent);
}

TEST(HTTPAPI, ServeDirOnlyServesFilesOfKnownMIMEType) {
  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FLAGS_net_api_test_tmpdir + "/wrong_static_files";
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("TXT is okay.", FileSystem::JoinPath(dir, "file.txt").c_str());
  FileSystem::WriteStringToFile("FOO is not! ", FileSystem::JoinPath(dir, "file.foo").c_str());
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).ServeStaticFilesFrom(dir), CannotServeStaticFilesOfUnknownMIMEType);
  FileSystem::RmDir(dir, FileSystem::RmDirParameters::Silent);
}

TEST(HTTPAPI, ResponseSmokeTest) {
  const auto send_response = [](const Response& response, Request request) { request(response); };

  const auto scope =
      HTTP(FLAGS_net_api_test_port)
          .Register("/response1", [send_response](Request r) { send_response(Response("foo"), std::move(r)); }) +
      HTTP(FLAGS_net_api_test_port)
          .Register(
              "/response2",
              [send_response](Request r) { send_response(Response("bar", HTTPResponseCode.Accepted), std::move(r)); }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response3",
                    [send_response](Request r) {
                      send_response(Response("baz", HTTPResponseCode.NotFound, "text/blah"), std::move(r));
                    }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response4",
                    [send_response](Request r) {
                      send_response(Response(SerializableObject(), HTTPResponseCode.Accepted), std::move(r));
                    }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response5",
                    [send_response](Request r) {
                      send_response(Response(SerializableObject(), "meh").Code(HTTPResponseCode.Created), std::move(r));
                    }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response6",
                    [send_response](Request r) {
                      send_response(Response().Body("OK").Code(HTTPResponseCode.OK), std::move(r));
                    }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response7",
                    [send_response](Request r) {
                      send_response(Response().JSON(SerializableObject(), "magic").Code(HTTPResponseCode.OK),
                                    std::move(r));
                    }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response8",
                    [send_response](Request r) { send_response(Response(HTTPResponseCode.Created), std::move(r)); }) +
      HTTP(FLAGS_net_api_test_port)
          .Register("/response9",
                    [send_response](Request r) {
                      send_response(Response(), std::move(r));  // Will result in a 500 "INTERNAL SERVER ERROR".
                    });

  const auto response1 = HTTP(GET(Printf("http://localhost:%d/response1", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response1.code));
  EXPECT_EQ("foo", response1.body);

  const auto response2 = HTTP(GET(Printf("http://localhost:%d/response2", FLAGS_net_api_test_port)));
  EXPECT_EQ(202, static_cast<int>(response2.code));
  EXPECT_EQ("bar", response2.body);

  const auto response3 = HTTP(GET(Printf("http://localhost:%d/response3", FLAGS_net_api_test_port)));
  EXPECT_EQ(404, static_cast<int>(response3.code));
  EXPECT_EQ("baz", response3.body);

  const auto response4 = HTTP(GET(Printf("http://localhost:%d/response4", FLAGS_net_api_test_port)));
  EXPECT_EQ(202, static_cast<int>(response4.code));
  EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response4.body);

  const auto response5 = HTTP(GET(Printf("http://localhost:%d/response5", FLAGS_net_api_test_port)));
  EXPECT_EQ(201, static_cast<int>(response5.code));
  EXPECT_EQ("{\"meh\":{\"x\":42,\"s\":\"foo\"}}\n", response5.body);

  const auto response6 = HTTP(GET(Printf("http://localhost:%d/response6", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response6.code));
  EXPECT_EQ("OK", response6.body);

  const auto response7 = HTTP(GET(Printf("http://localhost:%d/response7", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response7.code));
  EXPECT_EQ("{\"magic\":{\"x\":42,\"s\":\"foo\"}}\n", response7.body);

  const auto response8 = HTTP(GET(Printf("http://localhost:%d/response8", FLAGS_net_api_test_port)));
  EXPECT_EQ(201, static_cast<int>(response8.code));
  EXPECT_EQ("", response8.body);

  const auto response9 = HTTP(GET(Printf("http://localhost:%d/response9", FLAGS_net_api_test_port)));
  EXPECT_EQ(500, static_cast<int>(response9.code));
  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", response9.body);

  {
    Response response;
    response = "foo";
    response = SerializableObject();
    static_cast<void>(response);
  }
}

TEST(HTTPAPI, PayloadTooLarge) {
  const auto scope = HTTP(FLAGS_net_api_test_port)
                         .Register("/enough_is_enough",
                                   [](Request r) {
                                     ASSERT_FALSE(r.body.empty());
                                     EXPECT_EQ("POST", r.method);
                                     r("Fits.\n");
                                   });

  {
    const size_t size_ok = 16 * 1024 * 1024;
    ASSERT_EQ(current::net::constants::kMaxHTTPPayloadSizeInBytes, size_ok);

    const auto response =
        HTTP(POST(Printf("http://localhost:%d/enough_is_enough", FLAGS_net_api_test_port), std::string(size_ok, '.')));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("Fits.\n", response.body);
  }
  {
    const size_t size_too_much = 16 * 1024 * 1024 + 1;
    ASSERT_GT(size_too_much, current::net::constants::kMaxHTTPPayloadSizeInBytes);

    const auto response = HTTP(
        POST(Printf("http://localhost:%d/enough_is_enough", FLAGS_net_api_test_port), std::string(size_too_much, '.')));
    EXPECT_EQ(413, static_cast<int>(response.code));
    EXPECT_EQ("<h1>ENTITY TOO LARGE</h1>\n", response.body);
  }
}

CURRENT_STRUCT_T(HTTPAPITemplatedTestObject) {
  CURRENT_FIELD(text, std::string, "OK");
  CURRENT_FIELD(data, T);
};

struct HTTPAPINonSerializableObject {};

TEST(HTTPAPI, ResponseGeneratorForSerializableAndNonSerializableTypes) {
  const auto scope =
      HTTP(FLAGS_net_api_test_port)
          .Register(
              "/maybe_json",
              [](Request r) {
                if (r.url.query.has("json")) {
                  static_assert(current::serialization::json::IsJSONSerializable<HTTPAPITestObject>::value, "");
                  r(current::http::GenerateResponseFromMaybeSerializableObject<
                      HTTPAPITemplatedTestObject<HTTPAPITestObject> >(HTTPAPITemplatedTestObject<HTTPAPITestObject>()));
                } else {
                  static_assert(!current::serialization::json::IsJSONSerializable<HTTPAPINonSerializableObject>::value,
                                "");
                  r(current::http::GenerateResponseFromMaybeSerializableObject<
                      HTTPAPITemplatedTestObject<HTTPAPINonSerializableObject> >(
                      HTTPAPITemplatedTestObject<HTTPAPINonSerializableObject>()));
                }
              });
  {
    const auto response = HTTP(GET(Printf("http://localhost:%d/maybe_json?json", FLAGS_net_api_test_port)));
    EXPECT_EQ("{\"text\":\"OK\",\"data\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n", response.body);
    EXPECT_EQ(200, static_cast<int>(response.code));
  }
  {
    const auto response = HTTP(GET(Printf("http://localhost:%d/maybe_json", FLAGS_net_api_test_port)));
    EXPECT_EQ("", response.body);
    EXPECT_EQ(200, static_cast<int>(response.code));
  }
}

TEST(HTTPAPI, JSONHasOriginWhenSentViaRequest) {
  const auto scope = HTTP(FLAGS_net_api_test_port).Register("/json1", [](Request r) { r(SerializableObject()); });

  const auto response = HTTP(GET(Printf("http://localhost:%d/json1", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
  ASSERT_TRUE(response.headers.Has("Access-Control-Allow-Origin"));
  EXPECT_EQ("*", response.headers.Get("Access-Control-Allow-Origin"));
}

TEST(HTTPAPI, JSONHasOriginWhenSentViaResponse) {
  const auto scope =
      HTTP(FLAGS_net_api_test_port).Register("/json1", [](Request r) { r(Response(SerializableObject())); });

  const auto response = HTTP(GET(Printf("http://localhost:%d/json1", FLAGS_net_api_test_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
  ASSERT_TRUE(response.headers.Has("Access-Control-Allow-Origin"));
  EXPECT_EQ("*", response.headers.Get("Access-Control-Allow-Origin"));
}
