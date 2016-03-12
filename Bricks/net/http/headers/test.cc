/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// This `test.cc` is `#include`-d from `../test.cc`, so a "header" guard is in order.

#ifndef BRICKS_NET_HTTP_HEADERS_TEST_CC
#define BRICKS_NET_HTTP_HEADERS_TEST_CC

#include "../../../port.h"

#include "headers.h"

#include "../../../../3rdparty/gtest/gtest-main.h"

#include "../../../strings/join.h"
#include "../../../strings/printf.h"

using namespace current;

TEST(HTTPHeadersTest, Smoke) {
  using namespace current::net::http;  // `Headers`, `HeaderNotFoundException`.

  Headers headers;

  EXPECT_TRUE(headers.empty());
  EXPECT_EQ(0u, headers.size());
  EXPECT_FALSE(headers.Has("foo"));
  EXPECT_FALSE(headers.Has("bar"));
  EXPECT_EQ("nope", headers.GetOrDefault("foo", "nope"));
  EXPECT_EQ("nope", headers.GetOrDefault("bar", "nope"));
  ASSERT_THROW(headers.Get("foo"), HeaderNotFoundException);
  ASSERT_THROW(headers.Get("bar"), HeaderNotFoundException);

  headers.Set("foo", "1");
  EXPECT_FALSE(headers.empty());
  EXPECT_EQ(1u, headers.size());
  EXPECT_TRUE(headers.Has("foo"));
  EXPECT_FALSE(headers.Has("bar"));
  EXPECT_EQ("1", headers.GetOrDefault("foo", "nope"));
  EXPECT_EQ("nope", headers.GetOrDefault("bar", "nope"));
  EXPECT_EQ("1", headers.Get("foo"));
  ASSERT_THROW(headers.Get("bar"), HeaderNotFoundException);

  headers.Set("bar", "2");
  EXPECT_FALSE(headers.empty());
  EXPECT_EQ(2u, headers.size());
  EXPECT_TRUE(headers.Has("foo"));
  EXPECT_TRUE(headers.Has("bar"));
  EXPECT_EQ("1", headers.GetOrDefault("foo", "nope"));
  EXPECT_EQ("2", headers.GetOrDefault("bar", "nope"));
  EXPECT_EQ("1", headers.Get("foo"));
  EXPECT_EQ("2", headers.Get("bar"));
}

TEST(HTTPHeadersTest, Initialization) {
  using namespace current::net::http;  // `Headers`, `CookieIsNotYourRegularHeader`.

  {
    Headers headers("foo", "bar");
    EXPECT_EQ(1u, headers.size());
    EXPECT_EQ("bar", headers.Get("foo"));
  }

  {
    Headers headers({"foo", "bar"});
    EXPECT_EQ(1u, headers.size());
    EXPECT_EQ("bar", headers.Get("foo"));
  }

  {
    Headers headers({{"foo", "bar"}, {"baz", "meh"}});
    EXPECT_EQ(2u, headers.size());
    EXPECT_EQ("bar", headers.Get("foo"));
    EXPECT_EQ("meh", headers.Get("baz"));
  }

  {
    EXPECT_EQ(2u, Headers().Set("foo", "bar").Set("baz", "meh").size());
    EXPECT_EQ("bar", Headers().Set("foo", "bar").Set("baz", "meh").Get("foo"));
    EXPECT_EQ("meh", Headers().Set("foo", "bar").Set("baz", "meh").Get("baz"));
  }

  {
    ASSERT_THROW(Headers("Cookie", ""), CookieIsNotYourRegularHeader);
    ASSERT_THROW(Headers({"Cookie", ""}), CookieIsNotYourRegularHeader);
    ASSERT_THROW(Headers({{"Cookie", ""}}), CookieIsNotYourRegularHeader);
    ASSERT_THROW(Headers({{"a", "b"}, {"Cookie", ""}}), CookieIsNotYourRegularHeader);
  }
}

TEST(HTTPHeadersTest, PushBackAndEmplace) {
  using namespace current::net::http;  // `Headers`, `CookieIsNotYourRegularHeader`.

  {
    Headers headers;
    headers.push_back(std::make_pair("foo", "bar"));
    EXPECT_EQ(1u, headers.size());
    EXPECT_EQ("bar", headers.Get("foo"));
  }

  {
    Headers headers;
    headers.emplace_back("foo", "bar");
    EXPECT_EQ(1u, headers.size());
    EXPECT_EQ("bar", headers.Get("foo"));
  }

  {
    Headers headers;
    ASSERT_THROW(headers.push_back(std::make_pair("Cookie", "")), CookieIsNotYourRegularHeader);
    ASSERT_THROW(headers.emplace_back("Cookie", ""), CookieIsNotYourRegularHeader);
  }
}

TEST(HTTPHeadersTest, NormalizedNames) {
  using namespace current::net::http;  // `Headers`.

  Headers headers;

  headers.Set("example_header", "foo");

  // Case-insensitive, and '-' === '_'.
  EXPECT_EQ("foo", headers.Get("example_header"));
  EXPECT_EQ("foo", headers.Get("example-header"));
  EXPECT_EQ("foo", headers.Get("EXAMPLE_HEADER"));
  EXPECT_EQ("foo", headers.Get("EXAMPLE-HEADER"));
  EXPECT_EQ("foo", headers.Get("Example-Header"));
  EXPECT_EQ("foo", headers.Get("ExAmPlE-HeAdEr"));

  // Only the above conventions though.
  EXPECT_TRUE(headers.Has("example_header"));
  EXPECT_FALSE(headers.Has("example header"));
}

TEST(HTTPHeadersTest, AccumulatesHeaders) {
  using namespace current::net::http;  // `Headers`.

  Headers headers;

  EXPECT_FALSE(headers.Has("x"));

  // Set "x" = "foo".
  headers.Set("x", "foo");
  EXPECT_TRUE(headers.Has("x"));
  EXPECT_EQ("foo", headers.Get("x"));

  // Setting "x" to empty string should not have any effect.
  headers.Set("x", "");
  headers.Set("x", "");
  headers.Set("x", "");
  EXPECT_TRUE(headers.Has("x"));
  EXPECT_EQ("foo", headers.Get("x"));

  // Setting "x" to "bar" would concatenate "bar" with "foo" via ", ".
  headers.Set("x", "bar");
  EXPECT_TRUE(headers.Has("x"));
  EXPECT_EQ("foo, bar", headers.Get("x"));

  // Setting "x" to "baz" would concatenate "baz" with "foo, bar" via ", ".
  headers.Set("x", "baz");
  EXPECT_TRUE(headers.Has("x"));
  EXPECT_EQ("foo, bar, baz", headers.Get("x"));
}

TEST(HTTPHeadersTest, MapStyleAccess) {
  using namespace current::net::http;  // `Headers`, `HeaderNotFoundException`.

  Headers mutable_headers;
  const Headers& immutable_headers = mutable_headers;

  // Confirm `operator[]` throws when invoked on a const object for a non-existing header.
  EXPECT_FALSE(mutable_headers.Has("y"));
  EXPECT_FALSE(immutable_headers.Has("y"));
  ASSERT_THROW(immutable_headers["y"], HeaderNotFoundException);

  // Implicitly create an empty header.
  mutable_headers["y"];
  EXPECT_TRUE(mutable_headers.Has("y"));
  EXPECT_TRUE(mutable_headers.Has("Y"));
  EXPECT_EQ("", mutable_headers.Get("y"));
  EXPECT_EQ("", mutable_headers.Get("Y"));
  EXPECT_TRUE(immutable_headers.Has("y"));
  EXPECT_TRUE(immutable_headers.Has("Y"));
  EXPECT_EQ("", immutable_headers.Get("y"));
  EXPECT_EQ("", immutable_headers.Get("Y"));

  // Further access to this header should not change its value.
  mutable_headers["y"];
  mutable_headers["Y"];
  mutable_headers["y"];
  immutable_headers["y"];
  immutable_headers["Y"];
  immutable_headers["y"];
  EXPECT_TRUE(mutable_headers.Has("y"));
  EXPECT_TRUE(mutable_headers.Has("Y"));
  EXPECT_EQ("", mutable_headers.Get("y"));
  EXPECT_EQ("", mutable_headers.Get("Y"));

  // An empty value can be accessed via `operator[]`.
  EXPECT_EQ("", mutable_headers["y"].value);
  EXPECT_EQ("", mutable_headers["Y"].value);
  EXPECT_EQ("", immutable_headers["y"].value);
  EXPECT_EQ("", immutable_headers["Y"].value);

  // Test uppercase/lowercase too. The returned lowercase "y" is the capitalization of the name used first.
  EXPECT_EQ("y", mutable_headers["y"].header);
  EXPECT_EQ("y", mutable_headers["Y"].header);
  EXPECT_EQ("y", immutable_headers["y"].header);
  EXPECT_EQ("y", immutable_headers["Y"].header);

  // Modify this header gently, using `Set`.
  mutable_headers.Set("y", "foo");
  mutable_headers.Set("Y", "bar");
  EXPECT_EQ("foo, bar", mutable_headers.Get("y"));
  EXPECT_EQ("foo, bar", mutable_headers.Get("Y"));
  EXPECT_EQ("foo, bar", immutable_headers.Get("y"));
  EXPECT_EQ("foo, bar", immutable_headers.Get("Y"));
  EXPECT_EQ("foo, bar", mutable_headers["y"].value);
  EXPECT_EQ("foo, bar", mutable_headers["Y"].value);
  EXPECT_EQ("foo, bar", immutable_headers["y"].value);
  EXPECT_EQ("foo, bar", immutable_headers["Y"].value);

  // Change the header value forcefully, bypassing the concatenation via ", ".
  mutable_headers["Y"].value = "bla?";
  mutable_headers["Y"] = "bla!";
  EXPECT_EQ("bla!", mutable_headers.Get("y"));
  EXPECT_EQ("bla!", mutable_headers.Get("Y"));
  EXPECT_EQ("bla!", immutable_headers.Get("y"));
  EXPECT_EQ("bla!", immutable_headers.Get("Y"));
  EXPECT_EQ("bla!", mutable_headers["y"].value);
  EXPECT_EQ("bla!", mutable_headers["Y"].value);
  EXPECT_EQ("bla!", immutable_headers["y"].value);
  EXPECT_EQ("bla!", immutable_headers["Y"].value);

  // Confirm forceful update does not break the existing concatenation logic.
  mutable_headers.Set("Y", "PASS");
  EXPECT_EQ("bla!, PASS", mutable_headers.Get("y"));
  EXPECT_EQ("bla!, PASS", mutable_headers.Get("Y"));
  EXPECT_EQ("bla!, PASS", immutable_headers.Get("y"));
  EXPECT_EQ("bla!, PASS", immutable_headers.Get("Y"));
  EXPECT_EQ("bla!, PASS", mutable_headers["y"].value);
  EXPECT_EQ("bla!, PASS", mutable_headers["Y"].value);
  EXPECT_EQ("bla!, PASS", immutable_headers["y"].value);
  EXPECT_EQ("bla!, PASS", immutable_headers["Y"].value);

  // Smoke test.
  EXPECT_FALSE(mutable_headers.empty());
  EXPECT_FALSE(immutable_headers.empty());
  EXPECT_EQ(1u, mutable_headers.size());
  EXPECT_EQ(1u, immutable_headers.size());
}

TEST(HTTPHeadersTest, FirstUsedNameIsCanonical) {
  using namespace current::net::http;  // `Headers`.

  Headers mutable_headers;
  const Headers& immutable_headers = mutable_headers;

  // Create two headers and make a few changes to them.
  mutable_headers["FiRsT"].value = "1";
  mutable_headers["SeC-OnD"].value = "2";

  mutable_headers.Set("first", "one");
  mutable_headers.Set("sec_ond", "true");

  mutable_headers.Set("FIRST", "foo");
  mutable_headers.Set("SEC-OND", "bar");

  EXPECT_EQ(2u, mutable_headers.size());
  EXPECT_EQ(2u, immutable_headers.size());

  // Confirm originally used header names are retained.
  EXPECT_EQ("FiRsT", immutable_headers["first"].header);
  EXPECT_EQ("FiRsT", immutable_headers["First"].header);
  EXPECT_EQ("FiRsT", immutable_headers["FIRST"].header);
  EXPECT_EQ("FiRsT", immutable_headers["FiRsT"].header);
  EXPECT_EQ("SeC-OnD", immutable_headers["sec_ond"].header);
  EXPECT_EQ("SeC-OnD", immutable_headers["SEC-OND"].header);
  EXPECT_EQ("SeC-OnD", immutable_headers["SeC-OnD"].header);
  EXPECT_EQ("FiRsT", mutable_headers["first"].header);
  EXPECT_EQ("FiRsT", mutable_headers["First"].header);
  EXPECT_EQ("FiRsT", mutable_headers["FIRST"].header);
  EXPECT_EQ("FiRsT", mutable_headers["FiRsT"].header);
  EXPECT_EQ("SeC-OnD", mutable_headers["sec_ond"].header);
  EXPECT_EQ("SeC-OnD", mutable_headers["SEC-OND"].header);
  EXPECT_EQ("SeC-OnD", mutable_headers["SeC-OnD"].header);
}

TEST(HTTPHeadersTest, IterateOverHeaders) {
  using namespace current::net::http;  // `Headers`.

  Headers mutable_headers;
  const Headers& immutable_headers = mutable_headers;

  // Create threee headers.
  mutable_headers["header"].value = "1";
  mutable_headers["another"].value = "2";
  mutable_headers["Order-Is-Preserved"].value = "3";

  EXPECT_EQ(3u, mutable_headers.size());
  EXPECT_EQ(3u, immutable_headers.size());

  {
    std::vector<std::string> result;
    for (const auto& h : mutable_headers) {
      result.emplace_back(h.header + ":" + h.value);
    }
    EXPECT_EQ("header:1, another:2, Order-Is-Preserved:3", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    for (const auto& h : immutable_headers) {
      result.emplace_back(h.header + ":" + h.value);
    }
    EXPECT_EQ("header:1, another:2, Order-Is-Preserved:3", current::strings::Join(result, ", "));
  }

  // Modify headers using their non-canonical names, confirm order and canonical names.
  mutable_headers["order_is_preserved"].value = "three";
  mutable_headers["Header"].value = "one";
  mutable_headers["AnotheR"].value = "two";

  EXPECT_EQ(3u, mutable_headers.size());
  EXPECT_EQ(3u, immutable_headers.size());

  {
    std::vector<std::string> result;
    for (const auto& h : mutable_headers) {
      result.emplace_back(h.header + ":" + h.value);
    }
    EXPECT_EQ("header:one, another:two, Order-Is-Preserved:three", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    for (const auto& h : immutable_headers) {
      result.emplace_back(h.header + ":" + h.value);
    }
    EXPECT_EQ("header:one, another:two, Order-Is-Preserved:three", current::strings::Join(result, ", "));
  }
}

TEST(HTTPHeadersTest, DeepCopy) {
  using namespace current::net::http;  // `Headers`.

  Headers mutable_headers;
  const Headers& immutable_headers = mutable_headers;

  // Create threee headers.
  mutable_headers["header"].value = "1";
  mutable_headers["another"].value = "2";
  mutable_headers["Order-Is-Preserved"].value = "3";

  EXPECT_EQ(3u, mutable_headers.size());
  EXPECT_EQ(3u, immutable_headers.size());

  {
    Headers copy(immutable_headers);
    std::vector<std::string> result;
    for (const auto& h : copy) {
      result.emplace_back(h.header + ":" + h.value);
    }
    EXPECT_EQ("header:1, another:2, Order-Is-Preserved:3", current::strings::Join(result, ", "));
  }

  {
    Headers copy;
    copy = immutable_headers;
    std::vector<std::string> result;
    for (const auto& h : copy) {
      result.emplace_back(h.header + ":" + h.value);
    }
    EXPECT_EQ("header:1, another:2, Order-Is-Preserved:3", current::strings::Join(result, ", "));
  }
}

TEST(HTTPHeadersTest, CookiesSmokeTest) {
  using namespace current::net::http;  // `Headers`.

  Headers headers;
  EXPECT_TRUE(headers.cookies.empty());
  EXPECT_EQ("", headers.CookiesAsString());

  headers.ApplyCookieHeader("malformed_cookie");
  EXPECT_TRUE(headers.cookies.empty());

  headers.ApplyCookieHeader("cookie=yes");
  EXPECT_EQ(1u, headers.cookies.size());
  EXPECT_EQ(headers.cookies.begin()->first, "cookie");
  EXPECT_EQ(headers.cookies.begin()->second, "yes");
  EXPECT_EQ("cookie=yes", headers.CookiesAsString());

  headers.ApplyCookieHeader("another_cookie=oh_yes; this part is ignored");
  EXPECT_EQ(2u, headers.cookies.size());
  EXPECT_EQ(headers.cookies.begin()->first, "another_cookie");
  EXPECT_EQ(headers.cookies.begin()->second, "oh_yes");
  EXPECT_EQ("another_cookie=oh_yes; cookie=yes", headers.CookiesAsString());

  headers.SetCookie("fcuk", "yeah");
  EXPECT_EQ(3u, headers.cookies.size());
  EXPECT_EQ("another_cookie=oh_yes; cookie=yes; fcuk=yeah", headers.CookiesAsString());
}

TEST(HTTPHeadersTest, ShouldNotAccessCookiesAsRegularHeader) {
  using namespace current::net::http;  // `Headers`, `CookieIsNotYourRegularHeader`.

  Headers mutable_headers;
  const Headers& immutable_headers = mutable_headers;

  ASSERT_THROW(immutable_headers.Has("Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.Has("cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.Get("Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.Get("cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.GetOrDefault("Cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.GetOrDefault("cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers["Cookie"], CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers["cookie"], CookieIsNotYourRegularHeader);

  ASSERT_THROW(mutable_headers.Has("Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Has("cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Get("Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Get("cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.GetOrDefault("Cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.GetOrDefault("cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Set("Cookie", "yes, please"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Set("cookie", "yes, please"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers["Cookie"], CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers["cookie"], CookieIsNotYourRegularHeader);

  ASSERT_THROW(immutable_headers.Has("Set-Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.Has("set_cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.Get("Set-Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.Get("set_cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.GetOrDefault("Set-Cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers.GetOrDefault("set_cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers["Set-Cookie"], CookieIsNotYourRegularHeader);
  ASSERT_THROW(immutable_headers["set_cookie"], CookieIsNotYourRegularHeader);

  ASSERT_THROW(mutable_headers.Has("Set-Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Has("set_cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Get("Set-Cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Get("set_cookie"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.GetOrDefault("Set-Cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.GetOrDefault("set_cookie", ""), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Set("Set-Cookie", "yes, please"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers.Set("set_cookie", "yes, please"), CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers["Set-Cookie"], CookieIsNotYourRegularHeader);
  ASSERT_THROW(mutable_headers["set_cookie"], CookieIsNotYourRegularHeader);
}

#endif  // BRICKS_NET_HTTP_HEADERS_TEST_CC
