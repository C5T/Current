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

#include "../../dflags/dflags.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

using namespace bricks::net::url;

TEST(URLTest, SmokeTest) {
  URL u;

  u = URL("www.google.com");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);

  u = URL("www.google.com/test");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);

  u = URL("www.google.com:8080");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(8080, u.port);

  u = URL("meh://www.google.com:27960");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("meh", u.scheme);
  EXPECT_EQ(27960, u.port);

  u = URL("meh://www.google.com:27960/bazinga");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/bazinga", u.path);
  EXPECT_EQ("meh", u.scheme);
  EXPECT_EQ(27960, u.port);

  u = URL("localhost/");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);

  u = URL("localhost:/");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);

  u = URL("localhost/test");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);

  u = URL("localhost:/test");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
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

TEST(URLTest, DerivesSchemeFromPreviousPort) {
  // Smoke tests for non-default scheme, setting the 2nd parameter to the URL() constructor.
  EXPECT_EQ("www.google.com/", URL("www.google.com", "").ComposeURL());
  EXPECT_EQ("telnet://www.google.com:23/", URL("www.google.com", "telnet", "", 23).ComposeURL());
  // Keeps the scheme if it was explicitly specified, even for the port that maps to a different scheme.
  EXPECT_EQ("foo://www.google.com:80/", URL("foo://www.google.com", "", "", 80).ComposeURL());
  // Maps port 80 into "http://".
  EXPECT_EQ("http://www.google.com/", URL("www.google.com", "", "", 80).ComposeURL());
  // Maps port 443 into "https://".
  EXPECT_EQ("https://www.google.com/", URL("www.google.com", "", "", 443).ComposeURL());
  // Assumes port 80 for "http://".
  EXPECT_EQ("http://www.google.com:79/", URL("www.google.com", "http", "", 79).ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("www.google.com", "http", "", 80).ComposeURL());
  EXPECT_EQ("http://www.google.com:81/", URL("www.google.com", "http", "", 81).ComposeURL());
  // Assumes port 443 for "https://".
  EXPECT_EQ("https://www.google.com:442/", URL("www.google.com", "https", "", 442).ComposeURL());
  EXPECT_EQ("https://www.google.com/", URL("www.google.com", "https", "", 443).ComposeURL());
  EXPECT_EQ("https://www.google.com:444/", URL("www.google.com", "https", "", 444).ComposeURL());
  // Since there is no rule from "23" to "telnet", no scheme is specified.
  EXPECT_EQ("www.google.com:23/", URL("www.google.com", "", "", 23).ComposeURL());
}

TEST(URLTest, RedirectPreservesSchemeHostAndPortTest) {
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

TEST(URLTest, ExtractsURLParameters) {
  {
    URL u("www.google.com");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/", u.ComposeURL());
  }
  {
    URL u("www.google.com/a#fragment");
    EXPECT_EQ("fragment", u.fragment);
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/a#fragment", u.ComposeURL());
  }
  {
    URL u("www.google.com/a#fragment?foo=bar&baz=meh");
    EXPECT_EQ("fragment?foo=bar&baz=meh", u.fragment);
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/a#fragment?foo=bar&baz=meh", u.ComposeURL());
  }
  {
    URL u("www.google.com/b#fragment#foo");
    EXPECT_EQ("fragment#foo", u.fragment);
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/b#fragment#foo", u.ComposeURL());
  }
  {
    URL u("www.google.com/q?key=value&key2=value2#fragment#foo");
    EXPECT_EQ("fragment#foo", u.fragment);
    EXPECT_EQ("value", u.query["key"]);
    EXPECT_EQ("value", u.query.get("key", "default_value"));
    EXPECT_EQ("value2", u.query["key2"]);
    EXPECT_EQ("value2", u.query.get("key2", "default_value"));
    EXPECT_EQ("http://www.google.com/q?key=value&key2=value2#fragment#foo", u.ComposeURL());
  }
  {
    URL u("www.google.com/a?k=a%3Db%26s%3D%25s%23#foo");
    EXPECT_EQ("foo", u.fragment);
    EXPECT_EQ("a=b&s=%s#", u.query["k"]);
    EXPECT_EQ("http://www.google.com/a?k=a%3Db%26s%3D%25s%23#foo", u.ComposeURL());
  }
  {
    URL u("/q?key=value&key2=value2#fragment#foo");
    EXPECT_EQ("fragment#foo", u.fragment);
    EXPECT_EQ("value", u.query["key"]);
    EXPECT_EQ("value", u.query.get("key", "default_value"));
    EXPECT_EQ("value2", u.query["key2"]);
    EXPECT_EQ("value2", u.query.get("key2", "default_value"));
    EXPECT_EQ("/q?key=value&key2=value2#fragment#foo", u.ComposeURL());
  }
  {
    URL u("/a?k=a%3Db%26s%3D%25s%23#foo");
    EXPECT_EQ("foo", u.fragment);
    EXPECT_EQ("a=b&s=%s#", u.query["k"]);
    EXPECT_EQ("/a?k=a%3Db%26s%3D%25s%23#foo", u.ComposeURL());
  }
  {
    URL u("www.google.com/q?foo=&bar&baz=");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("", u.query["foo"]);
    EXPECT_EQ("", u.query.get("foo", "default_value"));
    EXPECT_EQ("", u.query["bar"]);
    EXPECT_EQ("", u.query.get("bar", "default_value"));
    EXPECT_EQ("", u.query["baz"]);
    EXPECT_EQ("", u.query.get("baz", "default_value"));
    EXPECT_EQ("http://www.google.com/q?foo=&bar=&baz=", u.ComposeURL());
  }
  {
    URL u("www.google.com/q?foo=bar=baz");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("bar=baz", u.query["foo"]);
    EXPECT_EQ("bar=baz", u.query.get("foo", "default_value"));
    EXPECT_EQ("http://www.google.com/q?foo=bar%3Dbaz", u.ComposeURL());
  }
  {
    URL u("www.google.com/q? foo = bar = baz ");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ(" bar = baz ", u.query[" foo "]);
    EXPECT_EQ(" bar = baz ", u.query.get(" foo ", "default_value"));
    EXPECT_EQ("http://www.google.com/q?%20foo%20=%20bar%20%3D%20baz%20", u.ComposeURL());
  }
  {
    URL u("www.google.com/q?1=foo");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("foo", u.query["1"]);
    EXPECT_EQ("foo", u.query.get("1", "default_value"));
    EXPECT_EQ("http://www.google.com/q?1=foo", u.ComposeURL());
  }
  {
    URL u("www.google.com/q?question=forty+two");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("forty two", u.query["question"]);
    EXPECT_EQ("forty two", u.query.get("question", "default_value"));
    EXPECT_EQ("http://www.google.com/q?question=forty%20two", u.ComposeURL());
  }
  {
    URL u("www.google.com/q?%3D+%3D=%3D%3D");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("==", u.query["= ="]);
    EXPECT_EQ("==", u.query.get("= =", "default_value"));
    EXPECT_EQ("http://www.google.com/q?%3D%20%3D=%3D%3D", u.ComposeURL());
  }
}

TEST(URLTest, URLParametersCompositionTest) {
  EXPECT_EQ("http://www.google.com/search", URL("www.google.com/search").ComposeURL());
  EXPECT_EQ("http://www.google.com/search?q=foo#fragment",
            URL("www.google.com/search?q=foo#fragment").ComposeURL());
  EXPECT_EQ("http://www.google.com/search?q=foo&q2=bar",
            URL("www.google.com/search?q=foo&q2=bar").ComposeURL());
  EXPECT_EQ("http://www.google.com/search?q=foo&q2=bar#fragment",
            URL("www.google.com/search?q=foo&q2=bar#fragment").ComposeURL());
  EXPECT_EQ("http://www.google.com/search#fragment", URL("www.google.com/search#fragment").ComposeURL());
}

TEST(URLTest, EmptyURLException) {
  // Empty URL should throw.
  ASSERT_THROW(URL(""), EmptyURLException);

  // Empty host is allowed in relative links.
  EXPECT_EQ("foo://www.website.com:321/second",
            URL("/second", URL("foo://www.website.com:321/first")).ComposeURL());
}
