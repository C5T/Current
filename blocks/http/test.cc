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

#include "../url/url.h"

#include "../../typesystem/struct.h"

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/strings/join.h"
#include "../../bricks/strings/printf.h"
#include "../../bricks/util/singleton.h"
#include "../../bricks/file/file.h"
#include "../../bricks/exception.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

using std::string;

using current::FileException;
using current::FileSystem;
using current::Singleton;
using current::strings::Printf;

using current::net::Connection;
using current::net::HTTPResponseCodeValue;
using current::net::http::Headers;

using current::net::DefaultInternalServerErrorMessage;
using current::net::DefaultMethodNotAllowedMessage;
using current::net::DefaultNotFoundMessage;

using current::net::HTTPRedirectLoopException;
using current::net::HTTPRedirectNotAllowedException;
using current::net::SocketResolveAddressException;

DEFINE_string(net_api_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

CURRENT_STRUCT(HTTPAPITestObject) {
  CURRENT_FIELD(number, int32_t);
  CURRENT_FIELD(text, std::string);
  CURRENT_FIELD(array, std::vector<int>);
  CURRENT_CONSTRUCTOR(HTTPAPITestObject)(int32_t number = 42, std::string text = "text", std::vector<int> array = {1, 2, 3})
    : number(number),
      text(std::move(text)),
      array(std::move(array)) {}
};

CURRENT_STRUCT(HTTPAPITestStructWithS) {
  CURRENT_FIELD(s, std::string);
  CURRENT_CONSTRUCTOR(HTTPAPITestStructWithS)(std::string s = "") : s(std::move(s)) {}
};

#if !defined(CURRENT_COVERAGE_REPORT_MODE) && !defined(CURRENT_WINDOWS)
TEST(ArchitectureTest, CURRENT_ARCH_UNAME_AS_IDENTIFIER) { ASSERT_EQ(CURRENT_ARCH_UNAME, FLAGS_current_runtime_arch); }
#endif

// Test the features of HTTP server.
TEST(HTTPAPI, Register) {
  using namespace current::http;
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  const auto scope = HTTP(std::move(reserved_port)).Register("/get", [](Request r) { r("OK"); });
  ASSERT_THROW(static_cast<void>(HTTP(port).Register("/get", nullptr)), HandlerAlreadyExistsException);
  const string url = Printf("http://localhost:%d/get", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(port).PathHandlersCount());
}

TEST(HTTPAPI, RegisterExceptions) {
  using namespace current::http;
  auto reserved_port = current::net::ReserveLocalPort();
  auto& http_server = HTTP(std::move(reserved_port));
  ASSERT_THROW(static_cast<void>(http_server.Register("no_slash", nullptr)), PathDoesNotStartWithSlash);
  ASSERT_THROW(static_cast<void>(http_server.Register("/wrong_slash/", nullptr)), PathEndsWithSlash);
  // The curly brackets are not necessarily wrong, but `URL::IsPathValidToRegister()` is `false` for them.
  ASSERT_THROW(static_cast<void>(http_server.Register("/{}", nullptr)), PathContainsInvalidCharacters);
}

TEST(HTTPAPI, RegisterWithURLPathParams) {
  using namespace current::http;
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;

  auto& http_server = HTTP(std::move(reserved_port));
  EXPECT_EQ(port, static_cast<int>(http_server.LocalPort()));

  const auto handler = [](Request r) {
    r(r.url.path + " (" + current::strings::Join(r.url_path_args, ", ") + ") " +
      (r.url_path_had_trailing_slash ? "url_path_had_trailing_slash" : ""));
  };

  const auto scope = HTTP(port).Register("/", URLPathArgs::CountMask::Any, handler) +
                     HTTP(port).Register("/user", URLPathArgs::CountMask::One | URLPathArgs::CountMask::Two, handler) +
                     HTTP(port).Register("/user/a", URLPathArgs::CountMask::One, handler) +
                     HTTP(port).Register("/user/a/1", URLPathArgs::CountMask::None, handler);

  ASSERT_THROW(static_cast<void>(HTTP(port).Register("/", handler)), HandlerAlreadyExistsException);
  ASSERT_THROW(static_cast<void>(HTTP(port).Register("/user", URLPathArgs::CountMask::Two, handler)),
               HandlerAlreadyExistsException);
  ASSERT_THROW(static_cast<void>(HTTP(port).Register("/user/a", URLPathArgs::CountMask::One, handler)),
               HandlerAlreadyExistsException);
  ASSERT_THROW(static_cast<void>(HTTP(port).Register("/user/a/1", handler)), HandlerAlreadyExistsException);

  const auto run = [port](const std::string& path) -> std::string {
    return HTTP(GET(Printf("http://localhost:%d", port) + path)).body;
  };

  EXPECT_EQ("/ () url_path_had_trailing_slash", run("/"));
  EXPECT_EQ("/ (foo) ", run("/foo"));
  EXPECT_EQ("/ (foo) url_path_had_trailing_slash", run("/foo/"));
  EXPECT_EQ("/ (foo, bar) ", run("/foo/bar"));
  EXPECT_EQ("/ (foo, bar) url_path_had_trailing_slash", run("/foo/bar/"));
  EXPECT_EQ("/ (user) ", run("/user"));
  EXPECT_EQ("/ (user) url_path_had_trailing_slash", run("/user/"));

  EXPECT_EQ("/ () url_path_had_trailing_slash", run("//"));
  EXPECT_EQ("/ () url_path_had_trailing_slash", run("///"));

  EXPECT_EQ("/user (a) ", run("/user/a"));
  EXPECT_EQ("/user (a) url_path_had_trailing_slash", run("/user/a/"));
  EXPECT_EQ("/user (a) ", run("/user///a"));
  EXPECT_EQ("/user (a) url_path_had_trailing_slash", run("/user///a///"));

  EXPECT_EQ("/user (x, y) ", run("/user/x/y"));
  EXPECT_EQ("/user (x, y) url_path_had_trailing_slash", run("/user/x/y/"));
  EXPECT_EQ("/user (x, y) ", run("/user///x//y"));
  EXPECT_EQ("/user (x, y) url_path_had_trailing_slash", run("/user///x//y//"));

  EXPECT_EQ("/user/a (0) ", run("/user/a/0"));
  EXPECT_EQ("/user/a (0) url_path_had_trailing_slash", run("/user/a/0/"));

  EXPECT_EQ("/user/a/1 () ", run("/user/a/1"));
  EXPECT_EQ("/user/a/1 () url_path_had_trailing_slash", run("/user/a/1/"));

  EXPECT_EQ("/user/a (2) ", run("/user/a/2"));
  EXPECT_EQ("/user/a (2) url_path_had_trailing_slash", run("/user/a/2/"));

  EXPECT_EQ("/ (user, a, 1, blah) ", run("/user/a/1/blah"));
  EXPECT_EQ("/ (user, a, 1, blah) url_path_had_trailing_slash", run("/user/a/1/blah/"));

  EXPECT_EQ("bar%20baz", URL::EncodeURIComponent("bar baz"));
  EXPECT_EQ("bar%2Fbaz", URL::EncodeURIComponent("bar/baz"));

  EXPECT_EQ("/ (foo, bar baz, meh) ", run("/foo/bar%20baz/meh"));
  EXPECT_EQ("/ (foo, bar/baz, meh) ", run("/foo/bar%2fbaz/meh"));
  EXPECT_EQ("/ (foo, bar/baz, meh) ", run("/foo/bar%2Fbaz/meh"));
}

TEST(HTTPAPI, ComposeURLPathWithURLPathArgs) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto handler = [](Request r) {
    r(r.url.path + " (" + r.url_path_args.ComposeURLPathFromArgs() + ", " + r.url_path_args.ComposeURLPath() + ")");
  };

  const auto scope = http_server.Register("/", URLPathArgs::CountMask::Any, handler) +
                     http_server.Register("/user", URLPathArgs::CountMask::One | URLPathArgs::CountMask::Two, handler) +
                     http_server.Register("/user/a", URLPathArgs::CountMask::One, handler) +
                     http_server.Register("/user/a/1", URLPathArgs::CountMask::None, handler);

  const auto run = [port](const std::string& path) -> std::string {
    return HTTP(GET(Printf("http://localhost:%d", port) + path)).body;
  };

  EXPECT_EQ("/ (/, /)", run("/"));
  EXPECT_EQ("/ (/foo, /foo)", run("/foo"));
  EXPECT_EQ("/ (/foo, /foo)", run("/foo/"));
  EXPECT_EQ("/ (/foo/bar, /foo/bar)", run("/foo/bar"));
  EXPECT_EQ("/ (/foo/bar, /foo/bar)", run("/foo/bar/"));
  EXPECT_EQ("/ (/user, /user)", run("/user"));
  EXPECT_EQ("/ (/user, /user)", run("/user/"));

  EXPECT_EQ("/ (/, /)", run("//"));
  EXPECT_EQ("/ (/, /)", run("///"));

  EXPECT_EQ("/user (/a, /user/a)", run("/user/a"));
  EXPECT_EQ("/user (/a, /user/a)", run("/user/a/"));
  EXPECT_EQ("/user (/a, /user/a)", run("/user///a"));
  EXPECT_EQ("/user (/a, /user/a)", run("/user///a///"));

  EXPECT_EQ("/user (/x/y, /user/x/y)", run("/user/x/y"));
  EXPECT_EQ("/user (/x/y, /user/x/y)", run("/user/x/y/"));
  EXPECT_EQ("/user (/x/y, /user/x/y)", run("/user///x//y"));
  EXPECT_EQ("/user (/x/y, /user/x/y)", run("/user///x//y//"));

  EXPECT_EQ("/user/a (/0, /user/a/0)", run("/user/a/0"));
  EXPECT_EQ("/user/a (/0, /user/a/0)", run("/user/a/0/"));

  EXPECT_EQ("/user/a/1 (/, /user/a/1)", run("/user/a/1"));
  EXPECT_EQ("/user/a/1 (/, /user/a/1)", run("/user/a/1/"));

  EXPECT_EQ("/user/a (/2, /user/a/2)", run("/user/a/2"));
  EXPECT_EQ("/user/a (/2, /user/a/2)", run("/user/a/2/"));

  EXPECT_EQ("/ (/user/a/1/blah, /user/a/1/blah)", run("/user/a/1/blah"));
  EXPECT_EQ("/ (/user/a/1/blah, /user/a/1/blah)", run("/user/a/1/blah/"));
}

TEST(HTTPAPI, ScopedUnRegister) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const string url = Printf("http://localhost:%d/foo", port);

  {
    EXPECT_EQ(0u, http_server.PathHandlersCount());
    HTTPRoutesScope registerer = http_server.Register("/foo", [](Request r) { r("bar"); });
    EXPECT_EQ(1u, http_server.PathHandlersCount());

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
    EXPECT_EQ("bar", HTTP(GET(url)).body);
  }

  {
    EXPECT_EQ(0u, http_server.PathHandlersCount());
    HTTPRoutesScope registerer;
    registerer += http_server.Register("/foo", [](Request r) { r("baz"); });
    EXPECT_EQ(1u, http_server.PathHandlersCount());

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
    EXPECT_EQ("baz", HTTP(GET(url)).body);
  }

  {
    EXPECT_EQ(0u, http_server.PathHandlersCount());
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(url)).code));
  }
}

TEST(HTTPAPI, ScopeCanBeAssignedNullPtr) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  auto scope = http_server.Register("/are_we_there_yet", [](Request r) { r("So far."); });
  const string url = Printf("http://localhost:%d/are_we_there_yet", port);
  EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
  scope = nullptr;
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(url)).code));
}

TEST(HTTPAPI, URLParameters) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/query", [](Request r) { r("x=" + r.url.query["x"]); });
  EXPECT_EQ("x=", HTTP(GET(Printf("http://localhost:%d/query", port))).body);
  EXPECT_EQ("x=42", HTTP(GET(Printf("http://localhost:%d/query?x=42", port))).body);
  EXPECT_EQ("x=test passed", HTTP(GET(Printf("http://localhost:%d/query?x=test+passed", port))).body);
  EXPECT_EQ("x=test passed", HTTP(GET(Printf("http://localhost:%d/query?x=test%%20passed", port))).body);
  EXPECT_EQ("x=test/passed", HTTP(GET(Printf("http://localhost:%d/query?x=test%%2fpassed", port))).body);
  EXPECT_EQ("x=test/passed", HTTP(GET(Printf("http://localhost:%d/query?x=test%%2Fpassed", port))).body);
}

TEST(HTTPAPI, InvalidHEXInURLParameters) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/qod", [](Request r) { r("wtf=" + r.url.query["wtf"]); });
  {
    const auto ok1 = HTTP(GET(Printf("http://localhost:%d/qod?wtf=OK", port)));
    EXPECT_EQ("wtf=OK", ok1.body);
    EXPECT_EQ(200, static_cast<int>(ok1.code));
  }
  {
    // Hexadecimal `4F4B` is 'OK'. Test both uppercase and lowercase.
    const auto ok2 = HTTP(GET(Printf("http://localhost:%d/qod?wtf=%%4F%%4b", port)));
    EXPECT_EQ("wtf=OK", ok2.body);
    EXPECT_EQ(200, static_cast<int>(ok2.code));
  }
  {
    // The presence of `%OK`, which is obviously wrong HEX code, in the URL should not kill the server.
    const auto ok3 = HTTP(GET(Printf("http://localhost:%d/qod?wtf=%%OK", port)));
    EXPECT_EQ(Printf("wtf=%%OK"), ok3.body);
    EXPECT_EQ(200, static_cast<int>(ok3.code));
  }
}

TEST(HTTPAPI, HeadersAndCookies) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/headers_and_cookies", [](Request r) {
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
  const auto response = HTTP(GET(Printf("http://localhost:%d/headers_and_cookies", port))
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/foo", [port](Request r) {
    const auto& c = r.connection;
    EXPECT_EQ("127.0.0.1", c.LocalIPAndPort().ip);
    EXPECT_EQ(port, c.LocalIPAndPort().port);
    EXPECT_EQ("127.0.0.1", c.RemoteIPAndPort().ip);
    EXPECT_LT(0, c.RemoteIPAndPort().port);
    r("bar", HTTPResponseCode.OK);
  });
  const string url = Printf("http://localhost:%d/foo", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("bar", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, http_server.PathHandlersCount());
}

TEST(HTTPAPI, RespondsWithString) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/responds_with_string", [](Request r) {
    r("test_string", HTTPResponseCode.OK, Headers({{"foo", "bar"}}), "application/json");
  });
  const string url = Printf("http://localhost:%d/responds_with_string", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("bar", response.headers.Get("foo"));
  EXPECT_EQ("test_string", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, http_server.PathHandlersCount());
}

TEST(HTTPAPI, RespondsWithObject) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/responds_with_object", [](Request r) {
    r(HTTPAPITestObject(), HTTPResponseCode.OK, Headers({{"foo", "bar"}}), "application/json");
  });
  const string url = Printf("http://localhost:%d/responds_with_object", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}\n", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, http_server.PathHandlersCount());
}

struct GoodStuff : current::http::IHasDoRespondViaHTTP {
  void DoRespondViaHTTP(Request r) const override {
    r("Good stuff.", HTTPResponseCode(762));  // https://github.com/joho/7XX-rfc
  }
};

TEST(HTTPAPI, RespondsWithCustomObject) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/dude_this_is_awesome", [](Request r) { r(GoodStuff()); });
  const string url = Printf("http://localhost:%d/dude_this_is_awesome", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(762, static_cast<int>(response.code));
  EXPECT_EQ("Good stuff.", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, http_server.PathHandlersCount());
}

TEST(HTTPAPI, HandlesRespondTwiceWithString) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/respond_twice", [](Request r) {
    r("OK");
    r("FAIL");
  });
  const string url = Printf("http://localhost:%d/respond_twice", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, HandlesRespondTwiceWithResponse) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  std::string result = "";
  std::atomic_bool result_ready(false);
  const auto scope = http_server.Register("/respond_twice", [&result, &result_ready](Request r) {
    r(Response("OK", HTTPResponseCode.OK));
    try {
      r(Response("FAIL", HTTPResponseCode(762)));
      result = "Error, second response did not throw.";
      result_ready = true;
    } catch (const current::net::AttemptedToSendHTTPResponseMoreThanOnce&) {
      result = "OK, second response did throw.";
      result_ready = true;
    }
  });
  const string url = Printf("http://localhost:%d/respond_twice", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(url, response.url);

  while (!result_ready) {
    std::this_thread::yield();
  }
  EXPECT_EQ("OK, second response did throw.", result);
}

#if !defined(CURRENT_APPLE) || defined(CURRENT_APPLE_HTTP_CLIENT_POSIX)
// Disabled redirect tests for Apple due to implementation specifics -- M.Z.
TEST(HTTPAPI, RedirectToRelativeURL) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/from", [](Request r) {
    r("", HTTPResponseCode.Found, Headers({{"Location", "/to"}}), current::net::constants::kDefaultHTMLContentType);
  }) + http_server.Register("/to", [](Request r) { r("Done."); });
  // Redirect not allowed by default.
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/from", port))), HTTPRedirectNotAllowedException);
  // Redirect allowed when `.AllowRedirects()` is set.
  const auto response = HTTP(GET(Printf("http://localhost:%d/from", port)).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Done.", response.body);
  EXPECT_EQ((port == 80 ? "http://localhost/to" : Printf("http://localhost:%d/to", port)), response.url);
}

TEST(HTTPAPI, RedirectToFullURL) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  auto second_reserved_port = current::net::ReserveLocalPort();
  const int second_port = second_reserved_port;

  // Need a live port for the redirect target because the HTTP client is following the redirect
  // and tries to connect to the redirect target, otherwise throws a `SocketConnectException`.
  const auto scope_redirect_to = HTTP(std::move(second_reserved_port)).Register("/to", [](Request r) { r("Done."); });
  const auto scope = http_server.Register("/from", [second_port](Request r) {
    r("",
      HTTPResponseCode.Found,
      Headers({{"Location", Printf("http://localhost:%d/to", second_port)}}),
      current::net::constants::kDefaultHTMLContentType);
  });
  // Redirect not allowed by default.
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/from", port))), HTTPRedirectNotAllowedException);
  // Redirect allowed when `.AllowRedirects()` is set.
  const auto response = HTTP(GET(Printf("http://localhost:%d/from", port)).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Done.", response.body);
  EXPECT_EQ((second_port == 80 ? "http://localhost/to" : Printf("http://localhost:%d/to", second_port)), response.url);
}

#if 0  // NOTE(dkorolev): Testing this too.
TEST(HTTPAPI, RedirectToFullURLWithoutPortRequiresRootAndOpenPort80) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  // WARNING: This test requires root access to bind to the reserved port `80`.
  // Need a live port for the redirect target because the HTTP client is following the redirect
  // and tries to connect to the redirect target, otherwise throws a `SocketConnectException`.
  const uint16_t default_http_port = 80;
  const auto scope_redirect_to = HTTP(current::net::BarePort(default_http_port)).Register("/to", [](Request r) { r("Done."); });
  const auto scope = http_server
                         .Register("/from",
                                   [](Request r) {
                                     r("",
                                       HTTPResponseCode.Found,
                                       Headers({{"Location", "http://localhost/to"}}),
                                       current::net::constants::kDefaultHTMLContentType);
                                   });
  // Redirect not allowed by default.
  ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/from", port))), HTTPRedirectNotAllowedException);
  // Redirect allowed when `.AllowRedirects()` is set.
  const auto response = HTTP(GET(Printf("http://localhost:%d/from", port)).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Done.", response.body);
  EXPECT_EQ("http://localhost/to", response.url);
}
#endif

TEST(HTTPAPI, RedirectLoop) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/p1", [](Request r) {
    r("", HTTPResponseCode.Found, Headers({{"Location", "/p2"}}), current::net::constants::kDefaultHTMLContentType);
  }) + http_server.Register("/p2", [](Request r) {
    r("", HTTPResponseCode.Found, Headers({{"Location", "/p3"}}), current::net::constants::kDefaultHTMLContentType);
  }) + http_server.Register("/p3", [](Request r) {
    r("", HTTPResponseCode.Found, Headers({{"Location", "/p1"}}), current::net::constants::kDefaultHTMLContentType);
  });
  {
    bool thrown = false;
    try {
      HTTP(GET(Printf("http://localhost:%d/p1", port)).AllowRedirects());
    } catch (HTTPRedirectLoopException& e) {
      thrown = true;
      std::string loop;
      if (port == 80) {
        loop += Printf("http://localhost/p1") + " ";
        loop += Printf("http://localhost/p2") + " ";
        loop += Printf("http://localhost/p3") + " ";
        loop += Printf("http://localhost/p1");
      } else {
        loop += Printf("http://localhost:%d/p1", port) + " ";
        loop += Printf("http://localhost:%d/p2", port) + " ";
        loop += Printf("http://localhost:%d/p3", port) + " ";
        loop += Printf("http://localhost:%d/p1", port);
      }
      EXPECT_EQ(loop, e.OriginalDescription());
    }
    EXPECT_TRUE(thrown);
  }
}
#endif

TEST(HTTPAPI, ResponseDotNotation) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/response_dot_notation", [](Request r) {
    r(Response("OK").Code(HTTPResponseCode.Created).SetHeader("X-Foo", "bar"));
  });
  const auto response = HTTP(GET(Printf("http://localhost:%d/response_dot_notation", port)));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(201, static_cast<int>(response.code));
  EXPECT_TRUE(response.headers.Has("X-Foo"));
  EXPECT_EQ("bar", response.headers.Get("X-Foo"));
}

static Response BuildResponse() { return Response("").Code(HTTPResponseCode.NoContent).SetHeader("X-Meh", "foo"); }

TEST(HTTPAPI, ResponseDotNotationReturnedFromAFunction) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/response_returned_from_function", [](Request r) { r(BuildResponse()); });
  const auto response = HTTP(GET(Printf("http://localhost:%d/response_returned_from_function", port)));
  EXPECT_EQ("", response.body);
  EXPECT_EQ(204, static_cast<int>(response.code));
  EXPECT_TRUE(response.headers.Has("X-Meh"));
  EXPECT_EQ("foo", response.headers.Get("X-Meh"));
}

TEST(HTTPAPI, FourOhFourNotFound) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;

  auto& http_server = HTTP(std::move(reserved_port));  // Start the server and do not register any handlers.
  static_cast<void>(http_server);

  EXPECT_EQ("<h1>NOT FOUND</h1>\n", DefaultNotFoundMessage());
  const string url = Printf("http://localhost:%d/ORLY", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(404, static_cast<int>(response.code));
  EXPECT_EQ(DefaultNotFoundMessage(), response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, FourOhFiveMethodNotAllowed) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  EXPECT_EQ("<h1>METHOD NOT ALLOWED</h1>\n", DefaultMethodNotAllowedMessage());
  const auto scope = http_server.Register("/method_not_allowed", [](Request r) {
    r(DefaultMethodNotAllowedMessage(),
      HTTPResponseCode.MethodNotAllowed,
      current::net::http::Headers(),
      current::net::constants::kDefaultHTMLContentType);
  });
  const string url = Printf("http://localhost:%d/method_not_allowed", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(405, static_cast<int>(response.code));
  EXPECT_EQ(DefaultMethodNotAllowedMessage(), response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, AnyMethodAllowed) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/foo", [](Request r) { r(r.method); });
  const string url = Printf("http://localhost:%d/foo", port);
  // A slightly more internal version to allow custom HTTP verb (request method).
  HTTPClientPOSIX client((current::http::impl::HTTPRedirectHelper::ConstructionParams()));
  client.request_method_ = "ANYTHING";
  client.request_url_ = url;
  ASSERT_TRUE(client.Go());
  // The current behavior does not validate the request method, delegates this to user-land code.
  EXPECT_EQ(200, static_cast<int>(client.response_code_));
  EXPECT_EQ("ANYTHING", client.HTTPRequest().Body());
}

TEST(HTTPAPI, DefaultInternalServerErrorCausedByExceptionInHandler) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", DefaultInternalServerErrorMessage());
  const auto scope = http_server.Register("/oh_snap", [](Request) {
    // Only `current::Exception` is caught and handled.
    CURRENT_THROW(current::Exception());
  });
  const string url = Printf("http://localhost:%d/oh_snap", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(500, static_cast<int>(response.code));
  EXPECT_EQ(DefaultInternalServerErrorMessage(), response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, HandlerIsCapturedByReference) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

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
  const auto scope = http_server.Register("/incr", helper) + http_server.Register("/incr_same", helper) +
                     http_server.Register("/incr_copy", copy);
  EXPECT_EQ("Incremented two.", HTTP(GET(Printf("http://localhost:%d/incr", port))).body);
  EXPECT_EQ(1u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  EXPECT_EQ("Incremented two.", HTTP(GET(Printf("http://localhost:%d/incr_same", port))).body);
  EXPECT_EQ(2u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  EXPECT_EQ("Incremented two.", HTTP(GET(Printf("http://localhost:%d/incr_copy", port))).body);
  EXPECT_EQ(2u, helper.counter);
  EXPECT_EQ(1u, copy.counter);
}

TEST(HTTPAPI, HandlerSupportsStaticMethods) {
  struct Static {
    static void Foo(Request r) { r("foo"); }
    static void Bar(Request r) { r("bar"); }
  };

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/foo", Static::Foo) + http_server.Register("/bar", Static::Bar);
  EXPECT_EQ("foo", HTTP(GET(Printf("http://localhost:%d/foo", port))).body);
  EXPECT_EQ("bar", HTTP(GET(Printf("http://localhost:%d/bar", port))).body);
}

// Don't wait 10 x 10ms beyond the 1st run when running tests in a loop.
struct ShouldReduceDelayBetweenChunksSingleton {
  bool yes = false;
};
// Test various HTTP client modes.
TEST(HTTPAPI, GetToFile) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/stars", [](Request r) {
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
  const string url = Printf("http://localhost:%d/stars?n=3", port);
  const auto response = HTTP(GET(url), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(file_name, response.body_file_name);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ("*ab12*ab12*ab12", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, ChunkedResponseWithHeaders) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/chunked_with_header", [](Request r) {
    EXPECT_EQ("GET", r.method);
    auto response =
        r.connection.SendChunkedHTTPResponse(HTTPResponseCode.OK, Headers({{"header", "yeah"}}), "text/plain");
    response.Send("A");
    response.Send("B");
    response.Send("C");
  });
  const auto response = HTTP(GET(Printf("http://localhost:%d/chunked_with_header", port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("ABC", response.body);
  ASSERT_TRUE(response.headers.Has("header"));
  EXPECT_EQ("yeah", response.headers.Get("header"));
}

TEST(HTTPAPI, GetByChunksPrototype) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  // Handler returning the result chunk by chunk.
  const auto scope = http_server.Register("/chunks", [](Request r) {
    auto response =
        r.connection.SendChunkedHTTPResponse(HTTPResponseCode.OK, Headers({{"header", "oh-well"}}), "text/plain");
    response.Send("1\n");
    response.Send("23\n");
    response.Send("456\n");
  });
  const string url = Printf("http://localhost:%d/chunks", port);
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
    const auto header_callback = [&headers](const std::string& k, const std::string& v) {
      headers.push_back(k + '=' + v);
    };
    const auto chunk_callback = [&chunk_by_chunk_response](const std::string& s) {
      chunk_by_chunk_response.push_back(s);
    };
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

    const auto response = HTTP(ChunkedGET(
        url,
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

TEST(HTTPAPI, ChunkedBodySemantics) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/test", [](Request r) {
    auto response = r.connection.SendChunkedHTTPResponse(HTTPResponseCode.OK,
                                                         Headers({{"TestHeaderName", "TestHeaderValue"}}),
                                                         current::net::constants::kDefaultJSONStreamContentType);
    std::string const bar = r.method == "POST" ? r.body : "bar";
    response.Send("{\"s\":");  // Lines intentionally broken into chunks the wrong way.
    response.Send("\"foo\"");
    response.Send("}\n{\"s\":\"" + bar + "\"}\n");
    response.Send("{\"s\":\"baz\"}");  // No newline by design.
  });

  const string url = Printf("http://localhost:%d/test", port);

  {
    const auto response = HTTP(GET(url));
    EXPECT_EQ(200, static_cast<int>(response.code));
    using S = HTTPAPITestStructWithS;
    EXPECT_EQ(JSON(S("foo")) + '\n' + JSON(S("bar")) + '\n' + JSON(S("baz")), response.body);
    EXPECT_EQ(4u, response.headers.size());
    EXPECT_EQ(
        "{"
        "\"Connection\":\"keep-alive\","
        "\"Content-Type\":\"application/stream+json; charset=utf-8\","
        "\"TestHeaderName\":\"TestHeaderValue\","
        "\"Transfer-Encoding\":\"chunked\""
        "}",
        JSON(response.headers.AsMap()));
  }

  {
    const auto response = HTTP(POST(url, "blah"));
    EXPECT_EQ(200, static_cast<int>(response.code));
    using S = HTTPAPITestStructWithS;
    EXPECT_EQ(JSON(S("foo")) + '\n' + JSON(S("blah")) + '\n' + JSON(S("baz")), response.body);
    EXPECT_EQ(4u, response.headers.size());
    EXPECT_EQ(
        "{"
        "\"Connection\":\"keep-alive\","
        "\"Content-Type\":\"application/stream+json; charset=utf-8\","
        "\"TestHeaderName\":\"TestHeaderValue\","
        "\"Transfer-Encoding\":\"chunked\""
        "}",
        JSON(response.headers.AsMap()));
  }

  {
    std::vector<std::string> headers;
    std::vector<std::string> chunk_by_chunk_response;

    const auto response = HTTP(ChunkedGET(
        url,
        [&headers](const std::string& k, const std::string& v) { headers.push_back(k + '=' + v); },
        [&chunk_by_chunk_response](const std::string& s) { chunk_by_chunk_response.push_back(s); },
        [&chunk_by_chunk_response]() { chunk_by_chunk_response.push_back("DONE"); }));
    EXPECT_EQ(200, static_cast<int>(response));
    EXPECT_EQ("{\"s\":|\"foo\"|}\n{\"s\":\"bar\"}\n|{\"s\":\"baz\"}|DONE",
              current::strings::Join(chunk_by_chunk_response, '|'));
    EXPECT_EQ(4u, headers.size());
    EXPECT_EQ(
        "Content-Type=application/stream+json; charset=utf-8|Connection=keep-alive"
        "|TestHeaderName=TestHeaderValue|Transfer-Encoding=chunked",
        current::strings::Join(headers, '|'));
  }

  {
    std::vector<std::string> headers;
    std::vector<std::string> chunk_by_chunk_response;

    const auto response =
        HTTP(ChunkedGET(url)
                 .OnHeader([&headers](const std::string& k, const std::string& v) { headers.push_back(k + '=' + v); })
                 .OnChunk([&chunk_by_chunk_response](const std::string& s) { chunk_by_chunk_response.push_back(s); })
                 .OnDone([&chunk_by_chunk_response]() { chunk_by_chunk_response.push_back("DONE"); }));
    EXPECT_EQ(200, static_cast<int>(response));
    EXPECT_EQ("{\"s\":|\"foo\"|}\n{\"s\":\"bar\"}\n|{\"s\":\"baz\"}|DONE",
              current::strings::Join(chunk_by_chunk_response, '|'));
    EXPECT_EQ(4u, headers.size());
    EXPECT_EQ(
        "Content-Type=application/stream+json; charset=utf-8|Connection=keep-alive"
        "|TestHeaderName=TestHeaderValue|Transfer-Encoding=chunked",
        current::strings::Join(headers, '|'));
  }

  {
    std::vector<std::string> headers;
    std::vector<std::string> chunk_by_chunk_response;

    const auto response =
        HTTP(ChunkedPOST(url, "test")
                 .OnHeader([&headers](const std::string& k, const std::string& v) { headers.push_back(k + '=' + v); })
                 .OnChunk([&chunk_by_chunk_response](const std::string& s) { chunk_by_chunk_response.push_back(s); })
                 .OnDone([&chunk_by_chunk_response]() { chunk_by_chunk_response.push_back("DONE"); }));
    EXPECT_EQ(200, static_cast<int>(response));
    EXPECT_EQ("{\"s\":|\"foo\"|}\n{\"s\":\"test\"}\n|{\"s\":\"baz\"}|DONE",
              current::strings::Join(chunk_by_chunk_response, '|'));
    EXPECT_EQ(4u, headers.size());
    EXPECT_EQ(
        "Content-Type=application/stream+json; charset=utf-8|Connection=keep-alive"
        "|TestHeaderName=TestHeaderValue|Transfer-Encoding=chunked",
        current::strings::Join(headers, '|'));
  }

  {
    std::vector<std::string> headers;
    std::vector<std::string> line_by_line_response;

    const auto response =
        HTTP(ChunkedPOST(url, "passed")
                 .OnHeader([&headers](const std::string& k, const std::string& v) { headers.push_back(k + '=' + v); })
                 .OnLine([&line_by_line_response](const std::string& s) { line_by_line_response.push_back(s); }));
    EXPECT_EQ(200, static_cast<int>(response));
    EXPECT_EQ("{\"s\":\"foo\"}|{\"s\":\"passed\"}|{\"s\":\"baz\"}", current::strings::Join(line_by_line_response, '|'));
    EXPECT_EQ(4u, headers.size());
    EXPECT_EQ(
        "Content-Type=application/stream+json; charset=utf-8|Connection=keep-alive"
        "|TestHeaderName=TestHeaderValue|Transfer-Encoding=chunked",
        current::strings::Join(headers, '|'));
  }

  {
    std::vector<std::string> headers;
    std::vector<std::string> parsed_body_pieces;
    using S = HTTPAPITestStructWithS;

    const auto response = HTTP(ChunkedPOST(url, "meh").OnLine([&parsed_body_pieces](const std::string& piece) {
      parsed_body_pieces.push_back(ParseJSON<S>(piece).s);
    }));
    EXPECT_EQ(200, static_cast<int>(response));
    EXPECT_EQ("foo,meh,baz", current::strings::Join(parsed_body_pieces, ','));
  }
}

TEST(HTTPAPI, PostFromBufferToBuffer) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    r("Data: " + r.body);
  });
  const auto response = HTTP(POST(Printf("http://localhost:%d/post", port), "No shit!", "application/octet-stream"));
  EXPECT_EQ("Data: No shit!", response.body);
}

TEST(HTTPAPI, PostAStringAsString) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post_string", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    EXPECT_EQ("POST", r.method);
    r(r.body);
  });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/post_string", port), std::string("std::string"), "text/plain")).body);
}

TEST(HTTPAPI, PostAStringAsConstCharPtr) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post_const_char_ptr", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    EXPECT_EQ("POST", r.method);
    r(r.body);
  });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/post_const_char_ptr", port),
                      static_cast<const char*>("const char*"),
                      "text/plain"))
                .body);
}

TEST(HTTPAPI, PostWithEmptyBodyMustSetZeroContentLength) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post", [](Request r) {
    ASSERT_TRUE(r.body.empty());
    r("Yo!\n");
  });
  const auto response = HTTP(POST(Printf("http://localhost:%d/post", port), ""));
  EXPECT_EQ("Yo!\n", response.body);
}

TEST(HTTPAPI, RespondWithStringAsString) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/respond_with_std_string", [](Request r) {
    EXPECT_EQ("POST", r.method);
    EXPECT_EQ("", r.body);
    r.connection.SendHTTPResponse(std::string("std::string"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("std::string", HTTP(POST(Printf("http://localhost:%d/respond_with_std_string", port), "")).body);
}

TEST(HTTPAPI, RespondWithStringAsConstCharPtr) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/respond_with_const_char_ptr", [](Request r) {
    EXPECT_EQ("", r.body);
    r.connection.SendHTTPResponse(static_cast<const char*>("const char*"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("const char*", HTTP(POST(Printf("http://localhost:%d/respond_with_const_char_ptr", port), "")).body);
}

TEST(HTTPAPI, RespondWithStringAsStringViaRequestDirectly) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/respond_with_std_string_via_request_directly", [](Request r) {
    EXPECT_EQ("", r.body);
    r(std::string("std::string"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("std::string",
            HTTP(POST(Printf("http://localhost:%d/respond_with_std_string_via_request_directly", port), "")).body);
}

TEST(HTTPAPI, RespondWithStringAsConstCharPtrViaRequestDirectly) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/respond_with_const_char_ptr_via_request_directly", [](Request r) {
    EXPECT_EQ("", r.body);
    r(static_cast<const char*>("const char*"), HTTPResponseCode.OK);
  });
  EXPECT_EQ("const char*",
            HTTP(POST(Printf("http://localhost:%d/respond_with_const_char_ptr_via_request_directly", port), "")).body);
}

CURRENT_STRUCT(SerializableObject) {
  CURRENT_FIELD(x, int32_t, 42);
  CURRENT_FIELD(s, std::string, "foo");
  std::string AsString() const { return Printf("%d:%s", static_cast<int>(x), s.c_str()); }
};

CURRENT_STRUCT(AnotherSerializableObject) { CURRENT_FIELD(z, uint32_t, 42u); };

CURRENT_VARIANT(SerializableVariant, SerializableObject, AnotherSerializableObject);

#if !defined(CURRENT_APPLE) || defined(CURRENT_APPLE_HTTP_CLIENT_POSIX)
// Disabled for Apple - native code doesn't throw exceptions -- M.Z.
TEST(HTTPAPI, PostFromInvalidFile) {
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string non_existent_file_name = FLAGS_net_api_test_tmpdir + "/non_existent_file";
  const auto test_file_scope = FileSystem::ScopedRmFile(non_existent_file_name);

  const int port = []() {
    auto reserved_port = current::net::ReserveLocalPort();
    const int port = reserved_port;
    return port;
  }();

  ASSERT_THROW(HTTP(POSTFromFile(Printf("http://localhost:%d/foo", port), non_existent_file_name, "text/plain")),
               FileException);
}
#endif

TEST(HTTPAPI, PostFromFileToBuffer) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    r("Voila: " + r.body);
  });
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_input_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/post", port);
  FileSystem::WriteStringToFile("No shit detected.", file_name.c_str());
  const auto response = HTTP(POSTFromFile(url, file_name, "application/octet-stream"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Voila: No shit detected.", response.body);
}

TEST(HTTPAPI, PostFromBufferToFile) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    r("Meh: " + r.body);
  });
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_output_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRmFile(file_name);
  const string url = Printf("http://localhost:%d/post", port);
  const auto response = HTTP(POST(url, "TEST BODY", "text/plain"), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Meh: TEST BODY", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, PostFromFileToFile) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    r("Phew: " + r.body);
  });
  current::FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const string request_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_request_test_file_for_http_post";
  const string response_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_response_test_file_for_http_post";
  const auto input_file_scope = FileSystem::ScopedRmFile(request_file_name);
  const auto output_file_scope = FileSystem::ScopedRmFile(response_file_name);
  const string url = Printf("http://localhost:%d/post", port);
  const string post_body = "Hi, this text should pass from one file to another. Mahalo!";
  FileSystem::WriteStringToFile(post_body, request_file_name.c_str());
  const auto response =
      HTTP(POSTFromFile(url, request_file_name, "text/plain"), SaveResponseToFile(response_file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Phew: Hi, this text should pass from one file to another. Mahalo!",
            FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, HeadRequest) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/head", [](Request r) {
    EXPECT_EQ("HEAD", r.method);
    ASSERT_TRUE(r.body.empty());
    r("", HTTPResponseCode.OK, Headers({{"foo", "bar"}}), "text/html");
  });
  const auto response = HTTP(HEAD(Printf("http://localhost:%d/head", port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_TRUE(response.body.empty());
  ASSERT_TRUE(response.headers.Has("foo"));
  EXPECT_EQ("bar", response.headers.Get("foo"));
}

TEST(HTTPAPI, DeleteRequest) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/delete", [](Request r) {
    EXPECT_EQ("DELETE", r.method);
    ASSERT_TRUE(r.body.empty());
    SerializableObject object;
    r(object);
  });
  const auto response = HTTP(DELETE(Printf("http://localhost:%d/delete", port)));
  EXPECT_EQ("42:foo", ParseJSON<SerializableObject>(response.body).AsString());
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, PatchRequest) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/patch", [](Request r) {
    EXPECT_EQ("PATCH", r.method);
    EXPECT_EQ("test", r.body);
    r("Patch OK.");
  });
  const auto response = HTTP(PATCH(Printf("http://localhost:%d/patch", port), "test"));
  EXPECT_EQ("Patch OK.", response.body);
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, PatchRequestWithJSON) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/patch", [](Request r) {
    EXPECT_EQ("PATCH", r.method);
    EXPECT_EQ("{\"number\":100,\"text\":\"foo\",\"array\":[1,2,3]}", r.body);
    ASSERT_TRUE(r.headers.Has("Content-Type"));
    EXPECT_EQ("application/json; charset=utf-8", r.headers.Get("Content-Type"));
    r("PATCH with JSON OK.");
  });
  const auto response = HTTP(PATCH(Printf("http://localhost:%d/patch", port), HTTPAPITestObject(100, "foo")));
  EXPECT_EQ("PATCH with JSON OK.", response.body);
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, POSTRequestWithVector) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post_with_vector", [](Request r) {
    EXPECT_EQ("POST", r.method);
    EXPECT_EQ("[101,102,103]", r.body);
    ASSERT_TRUE(r.headers.Has("Content-Type"));
    EXPECT_EQ("application/json; charset=utf-8", r.headers.Get("Content-Type"));
    r("POST with JSON Array OK.");
  });
  std::vector<int> v{101,102,103};
  const auto response = HTTP(POST(Printf("http://localhost:%d/post_with_vector", port), v));
  EXPECT_EQ("POST with JSON Array OK.", response.body);
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, POSTRequestWithMapStringInt) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post_with_map", [](Request r) {
    EXPECT_EQ("POST", r.method);
    EXPECT_EQ("{\"bar\":2,\"foo\":1}", r.body);
    ASSERT_TRUE(r.headers.Has("Content-Type"));
    EXPECT_EQ("application/json; charset=utf-8", r.headers.Get("Content-Type"));
    r("POST with JSON MapStringInt OK.");
  });
  std::map<std::string, int32_t> m;
  m["foo"] = 1;
  m["bar"] = 2;
  const auto response = HTTP(POST(Printf("http://localhost:%d/post_with_map", port), m));
  EXPECT_EQ("POST with JSON MapStringInt OK.", response.body);
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, POSTRequestWithMapIntString) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/post_with_map", [](Request r) {
    EXPECT_EQ("POST", r.method);
    EXPECT_EQ("[[0,\"the\"],[42,\"answer\"]]", r.body);  // TODO(dkorolev): Maybe revisit `map<non-string, T>` JSONs?
    ASSERT_TRUE(r.headers.Has("Content-Type"));
    EXPECT_EQ("application/json; charset=utf-8", r.headers.Get("Content-Type"));
    r("POST with JSON MapIntString OK.");
  });
  std::map<int32_t, std::string> m;
  m[42] = "answer";
  m[0] = "the";
  const auto response = HTTP(POST(Printf("http://localhost:%d/post_with_map", port), m));
  EXPECT_EQ("POST with JSON MapIntString OK.", response.body);
  EXPECT_EQ(200, static_cast<int>(response.code));
}

TEST(HTTPAPI, UserAgent) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope =
      http_server.Register("/ua", [](Request r) { r("TODO(dkorolev): Actually get passed in user agent."); });
  const string url = Printf("http://localhost:%d/ua", port);
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

TEST(HTTPAPI, ServeStaticFilesFrom) {
  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  const std::string sub_dir = FileSystem::JoinPath(dir, "sub_dir");
  const std::string sub_sub_dir = FileSystem::JoinPath(sub_dir, "sub_sub_dir");
  const std::string sub_dir_no_index = FileSystem::JoinPath(dir, "sub_dir_no_index");
  const std::string sub_dir_hidden = FileSystem::JoinPath(dir, ".sub_dir_hidden");
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_dir_no_index, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_dir_hidden, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML index</h1>", FileSystem::JoinPath(dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML file</h1>", FileSystem::JoinPath(dir, "file.html").c_str());
  FileSystem::WriteStringToFile("This is text.", FileSystem::JoinPath(dir, "file.txt").c_str());
  FileSystem::WriteStringToFile("\211PNG\r\n\032\n", FileSystem::JoinPath(dir, "file.png").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_dir index</h1>", FileSystem::JoinPath(sub_dir, "index.htm").c_str());
  FileSystem::WriteStringToFile("alert('JavaScript')", FileSystem::JoinPath(sub_dir, "file_in_sub_dir.js").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_sub_dir index</h1>",
                                FileSystem::JoinPath(sub_sub_dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML hidden</h1>", FileSystem::JoinPath(sub_dir_hidden, "index.html").c_str());
  FileSystem::WriteStringToFile("", FileSystem::JoinPath(dir, ".DS_Store").c_str());
  FileSystem::WriteStringToFile("", FileSystem::JoinPath(sub_dir, ".file_hidden").c_str());

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.ServeStaticFilesFrom(dir);

  // Root index file.
  {
    const auto root_response = HTTP(GET(Printf("http://localhost:%d/", port)));
    EXPECT_EQ(200, static_cast<int>(root_response.code));
    ASSERT_TRUE(root_response.headers.Has("Content-Type"));
    EXPECT_EQ("text/html", root_response.headers.Get("Content-Type"));
    EXPECT_EQ("<h1>HTML index</h1>", root_response.body);
  }

  // Root index file direct link.
  {
    const auto root_index_response = HTTP(GET(Printf("http://localhost:%d/index.html", port)));
    EXPECT_EQ(200, static_cast<int>(root_index_response.code));
    ASSERT_TRUE(root_index_response.headers.Has("Content-Type"));
    EXPECT_EQ("text/html", root_index_response.headers.Get("Content-Type"));
    EXPECT_EQ("<h1>HTML index</h1>", root_index_response.body);
  }

  // Misc files.
  {
    const auto html_response = HTTP(GET(Printf("http://localhost:%d/file.html", port)));
    EXPECT_EQ(200, static_cast<int>(html_response.code));
    ASSERT_TRUE(html_response.headers.Has("Content-Type"));
    EXPECT_EQ("text/html", html_response.headers.Get("Content-Type"));
    EXPECT_EQ("<h1>HTML file</h1>", html_response.body);
  }
  {
    const auto text_response = HTTP(GET(Printf("http://localhost:%d/file.txt", port)));
    EXPECT_EQ(200, static_cast<int>(text_response.code));
    ASSERT_TRUE(text_response.headers.Has("Content-Type"));
    EXPECT_EQ("text/plain", text_response.headers.Get("Content-Type"));
    EXPECT_EQ("This is text.", text_response.body);
  }
  {
    const auto png_response = HTTP(GET(Printf("http://localhost:%d/file.png", port)));
    EXPECT_EQ(200, static_cast<int>(png_response.code));
    ASSERT_TRUE(png_response.headers.Has("Content-Type"));
    EXPECT_EQ("image/png", png_response.headers.Get("Content-Type"));
    EXPECT_EQ("\211PNG\r\n\032\n", png_response.body);
  }

  // Redirect from directory without trailing slash to directory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/sub_dir", port))), HTTPRedirectNotAllowedException);
    const auto sub_dir_response = HTTP(GET(Printf("http://localhost:%d/sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_dir_response.code));
    EXPECT_EQ((port == 80 ? "http://localhost/sub_dir/" : Printf("http://localhost:%d/sub_dir/", port)),
              sub_dir_response.url);
    ASSERT_TRUE(sub_dir_response.headers.Has("Content-Type"));
    EXPECT_EQ("text/html", sub_dir_response.headers.Get("Content-Type"));
    EXPECT_EQ("<h1>HTML sub_dir index</h1>", sub_dir_response.body);
  }
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/sub_dir/sub_sub_dir", port))), HTTPRedirectNotAllowedException);
    const auto sub_sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/sub_dir/sub_sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_sub_dir_response.code));
    EXPECT_EQ((port == 80 ? "http://localhost/sub_dir/sub_sub_dir/"
                          : Printf("http://localhost:%d/sub_dir/sub_sub_dir/", port)),
              sub_sub_dir_response.url);
    ASSERT_TRUE(sub_sub_dir_response.headers.Has("Content-Type"));
    EXPECT_EQ("text/html", sub_sub_dir_response.headers.Get("Content-Type"));
    EXPECT_EQ("<h1>HTML sub_sub_dir index</h1>", sub_sub_dir_response.body);
  }

  // Subdirectory index file.
  EXPECT_EQ("<h1>HTML sub_dir index</h1>", HTTP(GET(Printf("http://localhost:%d/sub_dir/", port))).body);
  EXPECT_EQ("<h1>HTML sub_dir index</h1>", HTTP(GET(Printf("http://localhost:%d/sub_dir/index.htm", port))).body);

  // File in subdirectory.
  EXPECT_EQ("alert('JavaScript')", HTTP(GET(Printf("http://localhost:%d/sub_dir/file_in_sub_dir.js", port))).body);

  // Trailing slash for files should result in HTTP 404.
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/index.html/", port))).body);
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/sub_dir/index.htm/", port))).body);

  // Hidden files should result in HTTP 404.
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/.DS_Store", port))).body);
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/sub_dir/.file_hidden", port))).body);

  // Missing index file should result in HTTP 404.
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/sub_dir_no_index", port))).body);
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/sub_dir_no_index/", port))).body);

  // Hidden directory should result in HTTP 404.
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/.sub_dir_hidden", port))).body);
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/.sub_dir_hidden/", port))).body);
  EXPECT_EQ(DefaultNotFoundMessage(), HTTP(GET(Printf("http://localhost:%d/.sub_dir_hidden/index.html", port))).body);
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(Printf("http://localhost:%d/.sub_dir_hidden", port))).code));
  EXPECT_EQ(404, static_cast<int>(HTTP(POST(Printf("http://localhost:%d/.sub_dir_hidden/index.html", port), "")).code));

  // POST to file URL.
  EXPECT_EQ(DefaultMethodNotAllowedMessage(), HTTP(POST(Printf("http://localhost:%d/file.html", port), "")).body);
  EXPECT_EQ(405, static_cast<int>(HTTP(POST(Printf("http://localhost:%d/file.html", port), "")).code));

  // PUT to file URL.
  EXPECT_EQ(DefaultMethodNotAllowedMessage(), HTTP(PUT(Printf("http://localhost:%d/file.html", port), "")).body);
  EXPECT_EQ(405, static_cast<int>(HTTP(PUT(Printf("http://localhost:%d/file.html", port), "")).code));

  // PATCH to file URL.
  EXPECT_EQ(DefaultMethodNotAllowedMessage(), HTTP(PATCH(Printf("http://localhost:%d/file.html", port), "")).body);
  EXPECT_EQ(405, static_cast<int>(HTTP(PATCH(Printf("http://localhost:%d/file.html", port), "")).code));

  // DELETE to file URL.
  EXPECT_EQ(DefaultMethodNotAllowedMessage(), HTTP(DELETE(Printf("http://localhost:%d/file.html", port))).body);
  EXPECT_EQ(405, static_cast<int>(HTTP(DELETE(Printf("http://localhost:%d/file.html", port))).code));
}

TEST(HTTPAPI, ServeStaticFilesFromOptionsCustomRoutePrefix) {
  using namespace current::http;

  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  const std::string sub_dir = FileSystem::JoinPath(dir, "sub_dir");
  const std::string sub_sub_dir = FileSystem::JoinPath(sub_dir, "sub_sub_dir");
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML index</h1>", FileSystem::JoinPath(dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_dir index</h1>", FileSystem::JoinPath(sub_dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_sub_dir index</h1>",
                                FileSystem::JoinPath(sub_sub_dir, "index.html").c_str());

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.ServeStaticFilesFrom(dir, ServeStaticFilesFromOptions{"/static/something"});

  // Root index file.
  EXPECT_EQ("<h1>HTML index</h1>", HTTP(GET(Printf("http://localhost:%d/static/something/", port))).body);

  // Redirect from directory without trailing slash to directory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something", port))), HTTPRedirectNotAllowedException);
    const auto dir_response = HTTP(GET(Printf("http://localhost:%d/static/something", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(dir_response.code));
    EXPECT_EQ(
        (port == 80 ? "http://localhost/static/something/" : Printf("http://localhost:%d/static/something/", port)),
        dir_response.url);
    EXPECT_EQ("<h1>HTML index</h1>", dir_response.body);
  }

  // Subdirectory index file.
  EXPECT_EQ("<h1>HTML sub_dir index</h1>",
            HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/", port))).body);

  // Redirect from subdirectory without trailing slash to subdirectory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir", port))),
                 HTTPRedirectNotAllowedException);
    const auto sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_dir_response.code));
    EXPECT_EQ((port == 80 ? "http://localhost/static/something/sub_dir/"
                          : Printf("http://localhost:%d/static/something/sub_dir/", port)),
              sub_dir_response.url);
    EXPECT_EQ("<h1>HTML sub_dir index</h1>", sub_dir_response.body);
  }

  // Subsubdirectory index file.
  EXPECT_EQ("<h1>HTML sub_sub_dir index</h1>",
            HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir/", port))).body);

  // Redirect from subsubdirectory without trailing slash to subsubdirectory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir", port))),
                 HTTPRedirectNotAllowedException);
    const auto sub_sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_sub_dir_response.code));
    EXPECT_EQ((port == 80 ? "http://localhost/static/something/sub_dir/sub_sub_dir/"
                          : Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir/", port)),
              sub_sub_dir_response.url);
    EXPECT_EQ("<h1>HTML sub_sub_dir index</h1>", sub_sub_dir_response.body);
  }
}

TEST(HTTPAPI, ServeStaticFilesFromOptionsCustomRoutePrefixAndPublicUrlPrefixRelative) {
  using namespace current::http;

  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  const std::string sub_dir = FileSystem::JoinPath(dir, "sub_dir");
  const std::string sub_sub_dir = FileSystem::JoinPath(sub_dir, "sub_sub_dir");
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML index</h1>", FileSystem::JoinPath(dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_dir index</h1>", FileSystem::JoinPath(sub_dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_sub_dir index</h1>",
                                FileSystem::JoinPath(sub_sub_dir, "index.html").c_str());

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  // Below, use `localhost` and the secondary test port, not arbitrary domain name and port,
  // because the HTTP client follows the redirect and tries to resolve its target domain name and connect to the port.
  // This use case is for when there is a proxy, but we haven't set up one,
  // so after redirects, the response code and body are always HTTP 200 with "Done.", not the targeted content.
  const auto scope_redirect_to =
      http_server.Register("/anything", URLPathArgs::CountMask::Any, [](Request r) { r("Done."); });

  const auto scope =
      http_server.ServeStaticFilesFrom(dir, ServeStaticFilesFromOptions{"/static/something", "/anything"});

  // Root index file.
  EXPECT_EQ("<h1>HTML index</h1>", HTTP(GET(Printf("http://localhost:%d/static/something/", port))).body);

  // Redirect from directory without trailing slash to directory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something", port))), HTTPRedirectNotAllowedException);
    const auto dir_response = HTTP(GET(Printf("http://localhost:%d/static/something", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(dir_response.code));
    EXPECT_EQ((port == 80 ? "http://localhost/anything/" : Printf("http://localhost:%d/anything/", port)),
              dir_response.url);
    EXPECT_EQ("Done.", dir_response.body);
  }

  // Subdirectory index file.
  EXPECT_EQ("<h1>HTML sub_dir index</h1>",
            HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/", port))).body);

  // Redirect from subdirectory without trailing slash to subdirectory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir", port))),
                 HTTPRedirectNotAllowedException);
    const auto sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_dir_response.code));
    EXPECT_EQ(
        (port == 80 ? "http://localhost/anything/sub_dir/" : Printf("http://localhost:%d/anything/sub_dir/", port)),
        sub_dir_response.url);
    EXPECT_EQ("Done.", sub_dir_response.body);
  }

  // Subsubdirectory index file.
  EXPECT_EQ("<h1>HTML sub_sub_dir index</h1>",
            HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir/", port))).body);

  // Redirect from subsubdirectory without trailing slash to subsubdirectory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir", port))),
                 HTTPRedirectNotAllowedException);
    const auto sub_sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_sub_dir_response.code));
    EXPECT_EQ((port == 80 ? "http://localhost/anything/sub_dir/sub_sub_dir/"
                          : Printf("http://localhost:%d/anything/sub_dir/sub_sub_dir/", port)),
              sub_sub_dir_response.url);
    EXPECT_EQ("Done.", sub_sub_dir_response.body);
  }
}

TEST(HTTPAPI, ServeStaticFilesFromOptionsCustomRoutePrefixAndPublicUrlPrefixAbsolute) {
  using namespace current::http;

  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  const std::string sub_dir = FileSystem::JoinPath(dir, "sub_dir");
  const std::string sub_sub_dir = FileSystem::JoinPath(sub_dir, "sub_sub_dir");
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::MkDir(sub_sub_dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML index</h1>", FileSystem::JoinPath(dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_dir index</h1>", FileSystem::JoinPath(sub_dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML sub_sub_dir index</h1>",
                                FileSystem::JoinPath(sub_sub_dir, "index.html").c_str());

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  auto second_reserved_port = current::net::ReserveLocalPort();
  const int second_port = second_reserved_port;

  // Below, use `localhost` and the secondary test port, not arbitrary domain name and port,
  // because the HTTP client follows the redirect and tries to resolve its target domain name and connect to the port.
  // This use case is for when there is a proxy, but we haven't set up one,
  // so after redirects, the response code and body are always HTTP 200 with "Done.", not the targeted content.
  const auto scope_redirect_to =
      HTTP(std::move(second_reserved_port)).Register("/", URLPathArgs::CountMask::Any, [](Request r) { r("Done."); });

  const auto scope = http_server.ServeStaticFilesFrom(
      dir, ServeStaticFilesFromOptions{"/static/something", Printf("http://localhost:%d/anything", second_port)});

  // Root index file.
  EXPECT_EQ("<h1>HTML index</h1>", HTTP(GET(Printf("http://localhost:%d/static/something/", port))).body);

  // Redirect from directory without trailing slash to directory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something", port))), HTTPRedirectNotAllowedException);
    const auto dir_response = HTTP(GET(Printf("http://localhost:%d/static/something", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(dir_response.code));
    EXPECT_EQ((second_port == 80 ? "http://localhost/anything/" : Printf("http://localhost:%d/anything/", second_port)),
              dir_response.url);
    EXPECT_EQ("Done.", dir_response.body);
  }

  // Subdirectory index file.
  EXPECT_EQ("<h1>HTML sub_dir index</h1>",
            HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/", port))).body);

  // Redirect from subdirectory without trailing slash to subdirectory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir", port))),
                 HTTPRedirectNotAllowedException);
    const auto sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_dir_response.code));
    EXPECT_EQ((second_port == 80 ? "http://localhost/anything/sub_dir/"
                                 : Printf("http://localhost:%d/anything/sub_dir/", second_port)),
              sub_dir_response.url);
    EXPECT_EQ("Done.", sub_dir_response.body);
  }

  // Subsubdirectory index file.
  EXPECT_EQ("<h1>HTML sub_sub_dir index</h1>",
            HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir/", port))).body);

  // Redirect from subsubdirectory without trailing slash to subsubdirectory with trailing slash.
  {
    ASSERT_THROW(HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir", port))),
                 HTTPRedirectNotAllowedException);
    const auto sub_sub_dir_response =
        HTTP(GET(Printf("http://localhost:%d/static/something/sub_dir/sub_sub_dir", port)).AllowRedirects());
    EXPECT_EQ(200, static_cast<int>(sub_sub_dir_response.code));
    EXPECT_EQ((second_port == 80 ? "http://localhost/anything/sub_dir/sub_sub_dir/"
                                 : Printf("http://localhost:%d/anything/sub_dir/sub_sub_dir/", second_port)),
              sub_sub_dir_response.url);
    EXPECT_EQ("Done.", sub_sub_dir_response.body);
  }
}

TEST(HTTPAPI, ServeStaticFilesFromOptionsCustomRoutePrefixWithTrailingSlash) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  auto& http_server = HTTP(std::move(reserved_port));

  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  ASSERT_THROW(http_server.ServeStaticFilesFrom(dir, ServeStaticFilesFromOptions{"/static/"}), PathEndsWithSlash);
}

TEST(HTTPAPI, ServeStaticFilesFromOptionsEmptyPublicRoutePrefix) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  auto& http_server = HTTP(std::move(reserved_port));

  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  ASSERT_THROW(http_server.ServeStaticFilesFrom(dir, ServeStaticFilesFromOptions{""}), PathDoesNotStartWithSlash);
}

TEST(HTTPAPI, ServeStaticFilesFromOptionsCustomIndexFiles) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FileSystem::JoinPath(FLAGS_net_api_test_tmpdir, "static");
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("TXT index", FileSystem::JoinPath(dir, "index.txt").c_str());
  const auto scope = http_server.ServeStaticFilesFrom(dir, ServeStaticFilesFromOptions{"/", "", {"index.txt"}});
  EXPECT_EQ("TXT index", HTTP(GET(Printf("http://localhost:%d/", port))).body);
  EXPECT_EQ("TXT index", HTTP(GET(Printf("http://localhost:%d/index.txt", port))).body);
}

TEST(HTTPAPI, ServeStaticFilesFromOnlyServesOneIndexFilePerDirectory) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  auto& http_server = HTTP(std::move(reserved_port));

  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FLAGS_net_api_test_tmpdir + "/more_than_one_index";
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("<h1>HTML index 1</h1>", FileSystem::JoinPath(dir, "index.html").c_str());
  FileSystem::WriteStringToFile("<h1>HTML index 2</h1>", FileSystem::JoinPath(dir, "index.htm").c_str());
  ASSERT_THROW(http_server.ServeStaticFilesFrom(dir), ServeStaticFilesFromCannotServeMoreThanOneIndexFile);
}

TEST(HTTPAPI, ServeStaticFilesFromOnlyServesFilesOfKnownMIMEType) {
  using namespace current::http;

  auto reserved_port = current::net::ReserveLocalPort();
  auto& http_server = HTTP(std::move(reserved_port));

  FileSystem::MkDir(FLAGS_net_api_test_tmpdir, FileSystem::MkDirParameters::Silent);
  const std::string dir = FLAGS_net_api_test_tmpdir + "/wrong_static_files";
  const auto dir_remover = current::FileSystem::ScopedRmDir(dir);
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);
  FileSystem::WriteStringToFile("TXT is okay.", FileSystem::JoinPath(dir, "file.txt").c_str());
  FileSystem::WriteStringToFile("FOO is not! ", FileSystem::JoinPath(dir, "file.foo").c_str());
  ASSERT_THROW(http_server.ServeStaticFilesFrom(dir), ServeStaticFilesFromCanNotServeStaticFilesOfUnknownMIMEType);
}

TEST(HTTPAPI, ResponseSmokeTest) {
  const auto send_response = [](const Response& response, Request request) { request(response); };

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/response1", [send_response](Request r) {
    send_response(Response("foo"), std::move(r));
  }) + http_server.Register("/response2", [send_response](Request r) {
    send_response(Response("bar", HTTPResponseCode.Accepted), std::move(r));
  }) + http_server.Register("/response3", [send_response](Request r) {
    send_response(Response("baz", HTTPResponseCode.NotFound, "text/blah"), std::move(r));
  }) + http_server.Register("/response4", [send_response](Request r) {
    send_response(Response(SerializableObject(), HTTPResponseCode.Accepted), std::move(r));
  }) + http_server.Register("/response5", [send_response](Request r) {
    send_response(Response(SerializableObject(), "meh").Code(HTTPResponseCode.Created), std::move(r));
  }) + http_server.Register("/response6", [send_response](Request r) {
    send_response(Response().Body("OK").Code(HTTPResponseCode.OK), std::move(r));
  }) + http_server.Register("/response7", [send_response](Request r) {
    send_response(Response().JSON(SerializableObject(), "magic").Code(HTTPResponseCode.OK), std::move(r));
  }) + http_server.Register("/response8", [send_response](Request r) {
    send_response(Response(HTTPResponseCode.Created), std::move(r));
  }) + http_server.Register("/response9", [send_response](Request r) {
    send_response(Response(), std::move(r));  // Will result in a 500 "INTERNAL SERVER ERROR".
  }) + http_server.Register("/response10", [](Request r) {
    SerializableVariant v;
    v.template Construct<SerializableObject>();
    r(v);
  }) + http_server.Register("/response11", [send_response](Request r) {
    // Test both a direct response (`Request::operator()`) and a response via `Respose`.
    SerializableVariant v;
    v.template Construct<SerializableObject>();
    send_response(v, std::move(r));
  });

  const auto response1 = HTTP(GET(Printf("http://localhost:%d/response1", port)));
  EXPECT_EQ(200, static_cast<int>(response1.code));
  EXPECT_EQ("foo", response1.body);

  const auto response2 = HTTP(GET(Printf("http://localhost:%d/response2", port)));
  EXPECT_EQ(202, static_cast<int>(response2.code));
  EXPECT_EQ("bar", response2.body);

  const auto response3 = HTTP(GET(Printf("http://localhost:%d/response3", port)));
  EXPECT_EQ(404, static_cast<int>(response3.code));
  EXPECT_EQ("baz", response3.body);

  const auto response4 = HTTP(GET(Printf("http://localhost:%d/response4", port)));
  EXPECT_EQ(202, static_cast<int>(response4.code));
  EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response4.body);

  const auto response5 = HTTP(GET(Printf("http://localhost:%d/response5", port)));
  EXPECT_EQ(201, static_cast<int>(response5.code));
  EXPECT_EQ("{\"meh\":{\"x\":42,\"s\":\"foo\"}}\n", response5.body);

  const auto response6 = HTTP(GET(Printf("http://localhost:%d/response6", port)));
  EXPECT_EQ(200, static_cast<int>(response6.code));
  EXPECT_EQ("OK", response6.body);

  const auto response7 = HTTP(GET(Printf("http://localhost:%d/response7", port)));
  EXPECT_EQ(200, static_cast<int>(response7.code));
  EXPECT_EQ("{\"magic\":{\"x\":42,\"s\":\"foo\"}}\n", response7.body);

  const auto response8 = HTTP(GET(Printf("http://localhost:%d/response8", port)));
  EXPECT_EQ(201, static_cast<int>(response8.code));
  EXPECT_EQ("", response8.body);

  const auto response9 = HTTP(GET(Printf("http://localhost:%d/response9", port)));
  EXPECT_EQ(500, static_cast<int>(response9.code));
  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", response9.body);

  const auto response10 = HTTP(GET(Printf("http://localhost:%d/response10", port)));
  EXPECT_EQ(200, static_cast<int>(response10.code));
  EXPECT_EQ("{\"SerializableObject\":{\"x\":42,\"s\":\"foo\"},\"\":\"T9201749777787913665\"}\n", response10.body);

  const auto response11 = HTTP(GET(Printf("http://localhost:%d/response11", port)));
  EXPECT_EQ(200, static_cast<int>(response11.code));
  EXPECT_EQ("{\"SerializableObject\":{\"x\":42,\"s\":\"foo\"},\"\":\"T9201749777787913665\"}\n", response11.body);

  {
    Response response;
    response = "foo";
    response = SerializableObject();
    static_cast<void>(response);
  }
}

TEST(HTTPAPI, PayloadTooLarge) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/enough_is_enough", [](Request r) {
    ASSERT_FALSE(r.body.empty());
    EXPECT_EQ("POST", r.method);
    r("Fits.\n");
  });

  {
    const size_t size_ok = 16 * 1024 * 1024;
    ASSERT_EQ(current::net::constants::kMaxHTTPPayloadSizeInBytes, size_ok);

    const auto response = HTTP(POST(Printf("http://localhost:%d/enough_is_enough", port), std::string(size_ok, '.')));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("Fits.\n", response.body);
  }
  {
    const size_t size_too_much = 16 * 1024 * 1024 + 1;
    ASSERT_GT(size_too_much, current::net::constants::kMaxHTTPPayloadSizeInBytes);

    const auto response =
        HTTP(POST(Printf("http://localhost:%d/enough_is_enough", port), std::string(size_too_much, '.')));
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  const auto scope = http_server.Register("/maybe_json", [](Request r) {
    if (r.url.query.has("json")) {
      static_assert(current::serialization::json::IsJSONSerializable<HTTPAPITestObject>::value, "");
      r(current::http::GenerateResponseFromMaybeSerializableObject<HTTPAPITemplatedTestObject<HTTPAPITestObject>>(
          HTTPAPITemplatedTestObject<HTTPAPITestObject>()));
    } else {
      static_assert(!current::serialization::json::IsJSONSerializable<HTTPAPINonSerializableObject>::value, "");
      r(current::http::GenerateResponseFromMaybeSerializableObject<
          HTTPAPITemplatedTestObject<HTTPAPINonSerializableObject>>(
          HTTPAPITemplatedTestObject<HTTPAPINonSerializableObject>()));
    }
  });
  {
    const auto response = HTTP(GET(Printf("http://localhost:%d/maybe_json?json", port)));
    EXPECT_EQ("{\"text\":\"OK\",\"data\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n", response.body);
    EXPECT_EQ(200, static_cast<int>(response.code));
  }
  {
    const auto response = HTTP(GET(Printf("http://localhost:%d/maybe_json", port)));
    EXPECT_EQ("", response.body);
    EXPECT_EQ(200, static_cast<int>(response.code));
  }
}

TEST(HTTPAPI, JSONDoesNotHaveCORSHeaderByDefaultWhenSentViaRequest) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  {
    const auto scope = http_server.Register("/json1", [](Request r) { r(SerializableObject()); });

    const auto response = HTTP(GET(Printf("http://localhost:%d/json1", port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
    EXPECT_FALSE(response.headers.Has("Access-Control-Allow-Origin"));
  }
  {
    const auto scope = http_server.Register("/json2", [](Request r) {
      r(SerializableObject(), HTTPResponseCode.OK, current::net::http::Headers().SetCORSHeader());
    });
    const auto response = HTTP(GET(Printf("http://localhost:%d/json2", port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
    ASSERT_TRUE(response.headers.Has("Access-Control-Allow-Origin"));
    EXPECT_EQ("*", response.headers.Get("Access-Control-Allow-Origin"));
  }
  {
    const auto scope = http_server.Register("/json3", [](Request r) {
      r(SerializableObject(), HTTPResponseCode.OK, current::net::http::Headers().SetCORSHeader().RemoveCORSHeader());
    });

    const auto response = HTTP(GET(Printf("http://localhost:%d/json3", port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
    EXPECT_FALSE(response.headers.Has("Access-Control-Allow-Origin"));
  }
}

TEST(HTTPAPI, JSONDoesHaveCORSHeaderByDefault) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  {
    const auto scope = http_server.Register("/json1", [](Request r) { r(Response(SerializableObject())); });

    const auto response = HTTP(GET(Printf("http://localhost:%d/json1", port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
    ASSERT_TRUE(response.headers.Has("Access-Control-Allow-Origin"));
    EXPECT_EQ("*", response.headers.Get("Access-Control-Allow-Origin"));
  }
  {
    const auto scope =
        http_server.Register("/json2", [](Request r) { r(Response(SerializableObject()).DisableCORS()); });

    const auto response = HTTP(GET(Printf("http://localhost:%d/json2", port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
    EXPECT_FALSE(response.headers.Has("Access-Control-Allow-Origin"));
  }
  {
    const auto scope =
        http_server.Register("/json3", [](Request r) { r(Response(SerializableObject()).DisableCORS().EnableCORS()); });

    const auto response = HTTP(GET(Printf("http://localhost:%d/json3", port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("{\"x\":42,\"s\":\"foo\"}\n", response.body);
    ASSERT_TRUE(response.headers.Has("Access-Control-Allow-Origin"));
    EXPECT_EQ("*", response.headers.Get("Access-Control-Allow-Origin"));
  }
}
