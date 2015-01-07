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

#include "url.h"

#include "../../3party/gtest/gtest-main.h"

using namespace bricks::net::url;

TEST(URLTest, SmokeTest) {
  URL u;

  u = URL("www.google.com");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.protocol);
  EXPECT_EQ(80, u.port);

  u = URL("www.google.com/test");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.protocol);
  EXPECT_EQ(80, u.port);

  u = URL("www.google.com:8080");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.protocol);
  EXPECT_EQ(8080, u.port);

  u = URL("meh://www.google.com:27960");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("meh", u.protocol);
  EXPECT_EQ(27960, u.port);

  u = URL("meh://www.google.com:27960/bazinga");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/bazinga", u.path);
  EXPECT_EQ("meh", u.protocol);
  EXPECT_EQ(27960, u.port);

  u = URL("localhost:/test");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.protocol);
  EXPECT_EQ(80, u.port);
}

TEST(URLTest, CompositionTest) {
  EXPECT_EQ("http://www.google.com/", URL("www.google.com").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("www.google.com:80").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com:80").ComposeURL());
  EXPECT_EQ("http://www.google.com:8080/", URL("www.google.com:8080").ComposeURL());
  EXPECT_EQ("http://www.google.com:8080/", URL("http://www.google.com:8080").ComposeURL());
  EXPECT_EQ("meh://www.google.com:8080/", URL("meh://www.google.com:8080").ComposeURL());
}

TEST(URLTest, DerivesProtocolFromPreviousPort) {
  // Smoke tests for non-default protocol, setting the 2nd parameter to the URL() constructor.
  EXPECT_EQ("www.google.com/", URL("www.google.com", "").ComposeURL());
  EXPECT_EQ("telnet://www.google.com:23/", URL("www.google.com", "telnet", "", 23).ComposeURL());
  // Keeps the protocol if it was explicitly specified, even for the port that maps to a different protocol.
  EXPECT_EQ("foo://www.google.com:80/", URL("foo://www.google.com", "", "", 80).ComposeURL());
  // Maps port 80 into "http://".
  EXPECT_EQ("http://www.google.com/", URL("www.google.com", "", "", 80).ComposeURL());
  // Since there is no rule from "23" to "telnet", no protocol is specified.
  EXPECT_EQ("www.google.com:23/", URL("www.google.com", "", "", 23).ComposeURL());
}

TEST(URLTest, RedirectPreservesProtocolHostAndPortTest) {
  EXPECT_EQ("http://localhost/foo", URL("/foo", URL("localhost")).ComposeURL());
  EXPECT_EQ("meh://localhost/foo", URL("/foo", URL("meh://localhost")).ComposeURL());
  EXPECT_EQ("http://localhost:8080/foo", URL("/foo", URL("localhost:8080")).ComposeURL());
  EXPECT_EQ("meh://localhost:8080/foo", URL("/foo", URL("meh://localhost:8080")).ComposeURL());
  EXPECT_EQ("meh://localhost:27960/foo", URL(":27960/foo", URL("meh://localhost:8080")).ComposeURL());
  EXPECT_EQ("ftp://foo:8080/", URL("ftp://foo", URL("meh://localhost:8080")).ComposeURL());
  EXPECT_EQ("ftp://localhost:8080/bar", URL("ftp:///bar", URL("meh://localhost:8080")).ComposeURL());
  EXPECT_EQ("blah://new_host:5000/foo", URL("blah://new_host/foo", URL("meh://localhost:5000")).ComposeURL());
  EXPECT_EQ("blah://new_host:6000/foo",
            URL("blah://new_host:6000/foo", URL("meh://localhost:5000")).ComposeURL());
}

TEST(URLTest, EmptyURLException) {
  // Empty URL or host should throw.
  ASSERT_THROW(URL(""), EmptyURLException);
  ASSERT_THROW(URL("http://"), EmptyURLHostException);
  ASSERT_THROW(URL("http:///foo"), EmptyURLHostException);

  // Empty host is allowed in local links.
  EXPECT_EQ("foo://www.website.com:321/second",
            URL("/second", URL("foo://www.website.com:321/first")).ComposeURL());
}
