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

#include "../../Bricks/dflags/dflags.h"
#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/optional.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

using namespace current::url;

TEST(URLTest, SmokeTest) {
  URL u;

  // For URLs where no scheme or port is specified in the original string, no scheme or port is stored.

  // WARNING: Browser implementation differs: `www.google.com` is pathname.
  u = URL("www.google.com");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://www.google.com/", u.ComposeURL());

  // WARNING: Browser implementation differs: `www.google.com/test` is pathname.
  u = URL("www.google.com/test");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://www.google.com/test", u.ComposeURL());

  // WARNING: Browser implementation differs: `www.google.com` is protocol (scheme), `443/test` is pathname.
  u = URL("www.google.com:443");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(443, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("https://www.google.com/", u.ComposeURL());

  // WARNING: Browser implementation differs: `www.google.com` is protocol (scheme), `8080/test` is pathname.
  u = URL("www.google.com:8080");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(8080, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  // Unknown port `8080` falls back to default scheme in `ComposeURL`.
  EXPECT_EQ("http://www.google.com:8080/", u.ComposeURL());

  u = URL("meh://www.google.com:27960");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("meh", u.scheme);
  EXPECT_EQ(27960, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("meh://www.google.com:27960/", u.ComposeURL());

  u = URL("meh://www.google.com:27960/bazinga");
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/bazinga", u.path);
  EXPECT_EQ("meh", u.scheme);
  EXPECT_EQ(27960, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("meh://www.google.com:27960/bazinga", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost/` is pathname.
  u = URL("localhost/");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost/test` is pathname.
  u = URL("localhost/test");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/test", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost` is protocol (scheme), `/test` is pathname.
  u = URL("localhost:/");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost` is protocol (scheme), `/test` is pathname.
  u = URL("localhost:/test");
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/test", u.ComposeURL());

  // WARNING: Browser implementation differs: `:1234/test` is assumed pathname.
  u = URL(":1234/test");
  EXPECT_EQ("", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(1234, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  // NOTE: `ComposeURL` ignores the `port` if `host` is empty.
  EXPECT_EQ("/test", u.ComposeURL());

  u = URL("/test");
  EXPECT_EQ("", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(0, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("/test", u.ComposeURL());

  // Can parse a protocol-relative URL: `//host.name:80/path`.
  u = URL("//host.name:80/path");
  EXPECT_EQ("host.name", u.host);
  EXPECT_EQ("/path", u.path);
  EXPECT_EQ("", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  // NOTE: `ComposeURL` does not add the leading two slashes if scheme is empty.
  EXPECT_EQ("http://host.name/path", u.ComposeURL());

  // Can parse a username-password pair: `http://user1:pass345@host.name:80/path`.
  u = URL("http://user1:pass345@host.name:80/path");
  EXPECT_EQ("host.name", u.host);
  EXPECT_EQ("/path", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("user1", u.username);
  EXPECT_EQ("pass345", u.password);
  EXPECT_EQ("http://user1:pass345@host.name/path", u.ComposeURL());

  // Can parse a username alone: `http://user1@host.name:80/path`.
  u = URL("http://user1@host.name:80/path");
  EXPECT_EQ("host.name", u.host);
  EXPECT_EQ("/path", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("user1", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://user1@host.name/path", u.ComposeURL());
}

TEST(URLTest, FillWithDefaultsTest) {
  URL u;

  // For URLs where no scheme or port is specified in the original string, no scheme or port is stored.

  // WARNING: Browser implementation differs: `www.google.com` is pathname.
  u = URL("www.google.com").FillWithDefaults();
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://www.google.com/", u.ComposeURL());

  // WARNING: Browser implementation differs: `www.google.com/test` is pathname.
  u = URL("www.google.com/test").FillWithDefaults();
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://www.google.com/test", u.ComposeURL());

  // WARNING: Browser implementation differs: `www.google.com` is protocol (scheme), `443/test` is pathname.
  u = URL("www.google.com:443").FillWithDefaults();
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("https", u.scheme);
  EXPECT_EQ(443, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("https://www.google.com/", u.ComposeURL());

  // WARNING: Browser implementation differs: `www.google.com` is protocol (scheme), `8080/test` is pathname.
  u = URL("www.google.com:8080").FillWithDefaults();
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);  // Unknown port `8080` falls back to default scheme in `FillWithDefaults`.
  EXPECT_EQ(8080, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://www.google.com:8080/", u.ComposeURL());

  u = URL("meh://www.google.com:27960").FillWithDefaults();
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("meh", u.scheme);
  EXPECT_EQ(27960, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("meh://www.google.com:27960/", u.ComposeURL());

  u = URL("meh://www.google.com:27960/bazinga").FillWithDefaults();
  EXPECT_EQ("www.google.com", u.host);
  EXPECT_EQ("/bazinga", u.path);
  EXPECT_EQ("meh", u.scheme);
  EXPECT_EQ(27960, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("meh://www.google.com:27960/bazinga", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost/` is pathname.
  u = URL("localhost/").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost/test` is pathname.
  u = URL("localhost/test").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/test", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost` is protocol (scheme), `/test` is pathname.
  u = URL("localhost:/").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/", u.ComposeURL());

  // WARNING: Browser implementation differs: `localhost` is protocol (scheme), `/test` is pathname.
  u = URL("localhost:/test").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);

  u = URL("/test").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/test", u.ComposeURL());

  // WARNING: Browser implementation differs: `:1234/test` is assumed pathname.
  u = URL(":1234/test").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);  // Unknown port `1234` falls back to default scheme in `FillWithDefaults`.
  EXPECT_EQ(1234, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost:1234/test", u.ComposeURL());

  u = URL("/test").FillWithDefaults();
  EXPECT_EQ("localhost", u.host);
  EXPECT_EQ("/test", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://localhost/test", u.ComposeURL());

  u = URL("//hostname/path").FillWithDefaults();
  EXPECT_EQ("hostname", u.host);
  EXPECT_EQ("/path", u.path);
  EXPECT_EQ("http", u.scheme);
  EXPECT_EQ(80, u.port);
  EXPECT_EQ("", u.username);
  EXPECT_EQ("", u.password);
  EXPECT_EQ("http://hostname/path", u.ComposeURL());
}

TEST(URLTest, ComposeURLTest) {
  EXPECT_EQ("http://www.google.com/", URL("www.google.com").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("www.google.com:80").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com:80").ComposeURL());
  // Since there is no scheme and no rule for port "8080", default scheme is used by `ComposeURL`.
  EXPECT_EQ("http://www.google.com:8080/", URL("www.google.com:8080").ComposeURL());
  EXPECT_EQ("http://www.google.com:8080/", URL("http://www.google.com:8080").ComposeURL());
  EXPECT_EQ("meh://www.google.com:8080/", URL("meh://www.google.com:8080").ComposeURL());
}

TEST(URLTest, ComposeURLDerivesSchemeFromPort) {
  // Maps port 80 into "http://".
  EXPECT_EQ("http://www.google.com/", URL("www.google.com:80").ComposeURL());

  // Maps port 443 into "https://".
  EXPECT_EQ("https://www.google.com/", URL("www.google.com:443").ComposeURL());

  // Since there is no scheme and no rule for port "23", default scheme is used by `ComposeURL`.
  EXPECT_EQ("http://www.google.com:23/", URL("www.google.com:23").ComposeURL());

  // Keeps the scheme if it was explicitly specified, even for the port that maps to a different scheme.
  EXPECT_EQ("foo://www.google.com/", URL("foo://www.google.com").ComposeURL());
  EXPECT_EQ("foo://www.google.com:80/", URL("foo://www.google.com:80").ComposeURL());
  EXPECT_EQ("http://www.google.com:443/", URL("http://www.google.com:443").ComposeURL());

  // Assumes port 80 for "http://".
  EXPECT_EQ("http://www.google.com:79/", URL("http://www.google.com:79").ComposeURL());
  EXPECT_EQ("http://www.google.com/", URL("http://www.google.com:80").ComposeURL());
  EXPECT_EQ("http://www.google.com:81/", URL("http://www.google.com:81").ComposeURL());

  // Assumes port 443 for "https://".
  EXPECT_EQ("https://www.google.com:442/", URL("https://www.google.com:442").ComposeURL());
  EXPECT_EQ("https://www.google.com/", URL("https://www.google.com:443").ComposeURL());
  EXPECT_EQ("https://www.google.com:444/", URL("https://www.google.com:444").ComposeURL());
}

TEST(URLTest, RedirectToURLTest) {
  // Relative URL preserves scheme, host, port.
  EXPECT_EQ("http://localhost/foo", URL("localhost").RedirectToURL("/foo").ComposeURL());
  EXPECT_EQ("meh://localhost/foo", URL("meh://localhost").RedirectToURL("/foo").ComposeURL());
  EXPECT_EQ("http://localhost:8080/empty_all_with_previous_empty_scheme",
            URL("localhost:8080").RedirectToURL("/empty_all_with_previous_empty_scheme").ComposeURL());
  EXPECT_EQ("meh://localhost:8080/empty_all_with_previous_all",
            URL("meh://localhost:8080").RedirectToURL("/empty_all_with_previous_all").ComposeURL());

  // Port-only full URL replaces port and preserves host from previous URL.
  EXPECT_EQ("meh://localhost:27960/empty_scheme_host",
            URL("meh://localhost:8080").RedirectToURL(":27960/empty_scheme_host").ComposeURL());

  // Schema-only full URL preserves host and port from previous URL.
  EXPECT_EQ("ftp://localhost:8080/empty_host",
            URL("meh://localhost:8080").RedirectToURL("ftp:///empty_host").ComposeURL());

  // Full URL replaces scheme, host, port, does not preserve anything from previous URL.
  EXPECT_EQ("ftp://host_no_port_path/",
            URL("meh://localhost:8080").RedirectToURL("ftp://host_no_port_path").ComposeURL());
  EXPECT_EQ("blah://new_host/foo", URL("meh://localhost:5000").RedirectToURL("blah://new_host/foo").ComposeURL());
  EXPECT_EQ("blah://new_host:6000/foo",
            URL("meh://localhost:5000").RedirectToURL("blah://new_host:6000/foo").ComposeURL());

  // The username-password pair is always taken from the next URL.
  EXPECT_EQ("http://user1:pass345@new_host/foo",
            URL("meh://a:b@localhost:5000").RedirectToURL("http://user1:pass345@new_host/foo").ComposeURL());
}

TEST(URLTest, RedirectToURLWithParametersTest) {
  const auto redirected_url_parsed = URL("localhost/?replace=this&and=this").RedirectToURL("/foo?bar=baz&qoo=xdoo");
  EXPECT_EQ("http://localhost/foo?bar=baz&qoo=xdoo", redirected_url_parsed.ComposeURL());
  EXPECT_EQ("http://localhost/foo", redirected_url_parsed.ComposeURLWithoutParameters());

  EXPECT_EQ(
      "http://localhost:1234/foo?bar=baz&qoo=xdoo#with-fragment",
      URL("localhost:1234/?replace=this&and=this").RedirectToURL("/foo?bar=baz&qoo=xdoo#with-fragment").ComposeURL());

  const auto redirected_to_full_url_parsed =
      URL("localhost/?replace=this&and=this").RedirectToURL("http://other.domain/foo?bar=baz&qoo=xdoo");
  EXPECT_EQ("http://other.domain/foo?bar=baz&qoo=xdoo", redirected_to_full_url_parsed.ComposeURL());
  EXPECT_EQ("http://other.domain/foo", redirected_to_full_url_parsed.ComposeURLWithoutParameters());
}

TEST(URLTest, ExtractURLParametersAndComposeURLTest) {
  {
    URL u("www.google.com");
    EXPECT_EQ("", u.fragment);
    EXPECT_FALSE(u.query.has("key"));
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/", u.ComposeURLWithoutParameters());
  }
  {
    // Fragment is not passed through `DecodeURIComponent`.
    URL u("www.google.com/a#encoded-fragment-with_unreserved~chars!and%25reserved%3A%5Bchars%5D%2Band%20space");
    EXPECT_EQ("encoded-fragment-with_unreserved~chars!and%25reserved%3A%5Bchars%5D%2Band%20space", u.fragment);
    EXPECT_FALSE(u.query.has("key"));
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ(
        "http://www.google.com/a#encoded-fragment-with_unreserved~chars!and%25reserved%3A%5Bchars%5D%2Band%20space",
        u.ComposeURL());
    EXPECT_EQ("http://www.google.com/a", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/a#fragment?foo=bar&baz=meh");
    EXPECT_EQ("fragment?foo=bar&baz=meh", u.fragment);
    EXPECT_FALSE(u.query.has("key"));
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/a#fragment?foo=bar&baz=meh", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/a", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/b#fragment#foo");
    EXPECT_EQ("fragment#foo", u.fragment);
    EXPECT_FALSE(u.query.has("key"));
    EXPECT_EQ("", u.query["key"]);
    EXPECT_EQ("default_value", u.query.get("key", "default_value"));
    EXPECT_EQ("http://www.google.com/b#fragment#foo", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/b", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/q?key=value&key2=value2#fragment#foo");
    EXPECT_EQ("fragment#foo", u.fragment);
    EXPECT_TRUE(u.query.has("key"));
    EXPECT_EQ("value", u.query["key"]);
    EXPECT_EQ("value", u.query.get("key", "default_value"));
    EXPECT_TRUE(u.query.has("key2"));
    EXPECT_EQ("value2", u.query["key2"]);
    EXPECT_EQ("value2", u.query.get("key2", "default_value"));
    EXPECT_EQ("http://www.google.com/q?key=value&key2=value2#fragment#foo", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
    const auto& as_map = u.AllQueryParameters();
    EXPECT_EQ(2u, as_map.size());
    const auto key = as_map.find("key");
    const auto key2 = as_map.find("key2");
    const auto key3 = as_map.find("key3");
    ASSERT_TRUE(key != as_map.end());
    ASSERT_TRUE(key2 != as_map.end());
    ASSERT_TRUE(key3 == as_map.end());
    EXPECT_EQ("value", key->second);
    EXPECT_EQ("value2", key2->second);
  }
  {
    URL u("www.google.com/a?encoded%23query=-with_unreserved~chars!and%25reserved%3A%5Bchars%5Dplus%2Band%20space#foo");
    EXPECT_EQ("foo", u.fragment);
    EXPECT_TRUE(u.query.has("encoded#query"));
    EXPECT_EQ("-with_unreserved~chars!and%reserved:[chars]plus+and space", u.query["encoded#query"]);
    EXPECT_EQ(
        "http://www.google.com/"
        "a?encoded%23query=-with_unreserved~chars!and%25reserved%3A%5Bchars%5Dplus%2Band%20space#foo",
        u.ComposeURL());
    EXPECT_EQ("http://www.google.com/a", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/a?k=a%3Db%26s%3D%25s%23#foo");
    EXPECT_EQ("foo", u.fragment);
    EXPECT_EQ("a=b&s=%s#", u.query["k"]);
    EXPECT_EQ("http://www.google.com/a?k=a%3Db%26s%3D%25s%23#foo", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/a", u.ComposeURLWithoutParameters());
  }
  {
    URL u("/q?key=value&key2=value2#fragment#foo");
    EXPECT_EQ("fragment#foo", u.fragment);
    EXPECT_EQ("value", u.query["key"]);
    EXPECT_EQ("value", u.query.get("key", "default_value"));
    EXPECT_EQ("value2", u.query["key2"]);
    EXPECT_EQ("value2", u.query.get("key2", "default_value"));
    EXPECT_EQ("/q?key=value&key2=value2#fragment#foo", u.ComposeURL());
    EXPECT_EQ("/q", u.ComposeURLWithoutParameters());
  }
  {
    URL u("/a?k=a%3Db%26s%3D%25s%23#foo");
    EXPECT_EQ("foo", u.fragment);
    EXPECT_EQ("a=b&s=%s#", u.query["k"]);
    EXPECT_EQ("/a?k=a%3Db%26s%3D%25s%23#foo", u.ComposeURL());
    EXPECT_EQ("/a", u.ComposeURLWithoutParameters());
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
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/q?foo=bar=baz");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("bar=baz", u.query["foo"]);
    EXPECT_EQ("bar=baz", u.query.get("foo", "default_value"));
    EXPECT_EQ("http://www.google.com/q?foo=bar%3Dbaz", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/q? foo = bar = baz ");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ(" bar = baz ", u.query[" foo "]);
    EXPECT_EQ(" bar = baz ", u.query.get(" foo ", "default_value"));
    EXPECT_EQ("http://www.google.com/q?%20foo%20=%20bar%20%3D%20baz%20", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/q?1=foo");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("foo", u.query["1"]);
    EXPECT_EQ("foo", u.query.get("1", "default_value"));
    EXPECT_EQ("http://www.google.com/q?1=foo", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/q?question=forty+two");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("forty two", u.query["question"]);
    EXPECT_EQ("forty two", u.query.get("question", "default_value"));
    EXPECT_EQ("http://www.google.com/q?question=forty%20two", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
  }
  {
    URL u("www.google.com/q?%3D+%3D=%3D%3D");
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("==", u.query["= ="]);
    EXPECT_EQ("==", u.query.get("= =", "default_value"));
    EXPECT_EQ("http://www.google.com/q?%3D%20%3D=%3D%3D", u.ComposeURL());
    EXPECT_EQ("http://www.google.com/q", u.ComposeURLWithoutParameters());
  }
  {
    ASSERT_THROW(URL("?parameters=only"), EmptyURLException);
    ASSERT_THROW(URL("#fragment-only"), EmptyURLException);
  }
  {
    // The following construct can be used to parse the `application/x-www-form-urlencoded` request body.
    URL u("/?parameters=only");
    EXPECT_EQ("", u.scheme);
    EXPECT_EQ("", u.host);
    EXPECT_EQ(0, u.port);
    EXPECT_EQ("/", u.path);
    EXPECT_EQ("", u.fragment);
    EXPECT_EQ("only", u.query["parameters"]);
    EXPECT_EQ("/?parameters=only", u.ComposeURL());
    EXPECT_EQ("/", u.ComposeURLWithoutParameters());
  }
}

TEST(URLTest, EmptyURLException) {
  // Empty URL should throw.
  ASSERT_THROW(URL(""), EmptyURLException);

  // Empty host is allowed in relative links.
  EXPECT_EQ("/second", URL("/second").ComposeURL());
  // When redirected, the redirect path overrides the original path.
  EXPECT_EQ("foo://www.website.com:321/second",
            URL("foo://www.website.com:321/first").RedirectToURL("/second").ComposeURL());

  // With empty host, port is ignored.
  EXPECT_EQ("/second", URL(":1234/second").ComposeURL());
  // When redirected, the redirect port and path overrides the original port and path.
  EXPECT_EQ("foo://www.website.com:1234/second",
            URL("foo://www.website.com:321/first").RedirectToURL(":1234/second").ComposeURL());
}

namespace url_test {

CURRENT_STRUCT(Simple) {
  CURRENT_FIELD(a, int64_t);
  CURRENT_FIELD(b, int64_t);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(z, bool);
};

CURRENT_STRUCT(SimpleWithOptionals) {
  CURRENT_FIELD(a, int64_t);
  CURRENT_FIELD(b, Optional<int64_t>);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, Optional<std::string>);
};

CURRENT_STRUCT(Tricky) {
  CURRENT_FIELD(s, Optional<std::string>);
  CURRENT_FIELD(p, (std::pair<std::string, std::string>));
  CURRENT_FIELD(v, std::vector<std::string>);
  CURRENT_FIELD(m, (std::map<std::string, std::string>));
  CURRENT_FIELD(z, Optional<bool>);
  CURRENT_FIELD(q, Optional<Simple>);
};

}  // namespace url_test

TEST(URLTest, FillsCurrentStructsFromURLParameters) {
  using namespace url_test;

  {
    const auto simple = URL("/simple?a=1&b=2&s=test with spaces").query.template FillObject<Simple>();
    EXPECT_EQ(1, simple.a);
    EXPECT_EQ(2, simple.b);
    EXPECT_EQ("test with spaces", simple.s);
  }
  {
    EXPECT_TRUE(URL("/simple?z=1").query.template FillObject<Simple>().z);
    EXPECT_TRUE(URL("/simple?z=true").query.template FillObject<Simple>().z);
    EXPECT_TRUE(URL("/simple?z=True").query.template FillObject<Simple>().z);
    EXPECT_TRUE(URL("/simple?z=TRUE").query.template FillObject<Simple>().z);
    EXPECT_TRUE(URL("/simple?z").query.template FillObject<Simple>().z);
    EXPECT_FALSE(URL("/simple?z=0").query.template FillObject<Simple>().z);
    EXPECT_FALSE(URL("/simple?z=false").query.template FillObject<Simple>().z);
    EXPECT_FALSE(URL("/simple?z=False").query.template FillObject<Simple>().z);
    EXPECT_FALSE(URL("/simple?z=FALSE").query.template FillObject<Simple>().z);

    // Anything but `0`, `false`, `False`, or `FALSE` is treated as true.
    EXPECT_TRUE(URL("/simple?z=something_not_false").query.template FillObject<Simple>().z);
  }

  {
    // When parsing top-level URL parameters, the missing ones are plain ignored.
    Simple simple;
    simple.a = 42;
    URL("/simple").query.template FillObject(simple);
    EXPECT_EQ(42, simple.a);
  }
  {
    // But when the top-level URL parameter is present yet unparsable, it's an exception.
    try {
      URL("/simple?a=not a number").query.FillObject<Simple>();
      ASSERT_TRUE(false);
    } catch (const URLParseObjectAsURLParameterException& e) {
      EXPECT_EQ("a", e.key);
      EXPECT_EQ("not a number", e.error);
    }
  }

  {
    try {
      URL("/test").query.template FillObject<SimpleWithOptionals, current::url::FillObjectMode::Strict>();
      ASSERT_TRUE(false);
    } catch (const URLParseObjectAsURLParameterException& e) {
      EXPECT_EQ("a", e.key);
      EXPECT_EQ("missing value", e.error);
    }
  }

  {
    try {
      URL("/test?a=42").query.template FillObject<SimpleWithOptionals, current::url::FillObjectMode::Strict>();
      ASSERT_TRUE(false);
    } catch (const URLParseObjectAsURLParameterException& e) {
      EXPECT_EQ("s", e.key);
      EXPECT_EQ("missing value", e.error);
    }
  }

  {
    const auto object =
        URL("/test?a=42&s=foo").query.template FillObject<SimpleWithOptionals, current::url::FillObjectMode::Strict>();
    EXPECT_EQ(42, object.a);
    EXPECT_EQ("foo", object.s);
    EXPECT_FALSE(Exists(object.b));
    EXPECT_FALSE(Exists(object.t));
  }

  {
    SimpleWithOptionals object;
    // These values won't be touched.
    object.b = 10000;
    object.t = "bar";
    URL("/test?a=42&s=foo")
        .query.template FillObject<SimpleWithOptionals, current::url::FillObjectMode::Strict>(object);
    EXPECT_EQ(42, object.a);
    EXPECT_EQ("foo", object.s);
    EXPECT_TRUE(Exists(object.b));
    EXPECT_TRUE(Exists(object.t));
    EXPECT_EQ(10000, Value(object.b));
    EXPECT_EQ("bar", Value(object.t));
  }

  {
    const auto object = URL("/test?a=42&b=43&s=foo&t=baz")
                            .query.template FillObject<SimpleWithOptionals, current::url::FillObjectMode::Strict>();
    EXPECT_EQ(42, object.a);
    EXPECT_EQ("foo", object.s);
    EXPECT_TRUE(Exists(object.b));
    EXPECT_TRUE(Exists(object.t));
    EXPECT_EQ(43, Value(object.b));
    EXPECT_EQ("baz", Value(object.t));
  }

  {
    Tricky tricky;

    URL("/tricky").query.FillObject(tricky);
    EXPECT_FALSE(Exists(tricky.s));

    URL("/tricky?s=foo").query.FillObject(tricky);
    ASSERT_TRUE(Exists(tricky.s));
    EXPECT_EQ("foo", Value(tricky.s));

    URL("/tricky?s=").query.FillObject(tricky);
    ASSERT_TRUE(Exists(tricky.s));
    EXPECT_EQ("", Value(tricky.s));

    URL("/tricky?p=[\"bar\",\"baz\"]").query.FillObject(tricky);
    EXPECT_EQ("bar", tricky.p.first);
    EXPECT_EQ("baz", tricky.p.second);

    URL("/tricky?v=[\"test\",\"gloriously\\npassed\"]").query.FillObject(tricky);
    ASSERT_EQ(2u, tricky.v.size());
    EXPECT_EQ("test", tricky.v[0]);
    EXPECT_EQ("gloriously\npassed", tricky.v[1]);

    URL("/tricky?m={\"key\":\"value\",\"works\":\"indeed\"}").query.FillObject(tricky);
    ASSERT_EQ(2u, tricky.m.size());
    EXPECT_EQ("value", tricky.m["key"]);
    EXPECT_EQ("indeed", tricky.m["works"]);

    // When parsing JSON-s, the complete JSON should be valid.
    // Unlike top-level URL parametrers, a field missing inside a JSON results in an exception.
    try {
      URL("/tricky?q={\"a\":\"not a number\"}").query.FillObject<Tricky>();
      ASSERT_TRUE(false);
    } catch (const URLParseObjectAsURLParameterException& e) {
      EXPECT_EQ("q", e.key);
      EXPECT_EQ("Expected integer for `a`, got: \"not a number\"", e.error);
    }

    try {
      URL("/tricky?q={\"b\":\"not a number\"}").query.FillObject<Tricky>();
      ASSERT_TRUE(false);
    } catch (const URLParseObjectAsURLParameterException& e) {
      EXPECT_EQ("q", e.key);
      EXPECT_EQ("Expected integer for `a`, got: missing field.", e.error);
    }

    try {
      URL("/tricky?q={\"a\":42,\"b\":\"not a number\"}").query.FillObject<Tricky>();
      ASSERT_TRUE(false);
    } catch (const URLParseObjectAsURLParameterException& e) {
      EXPECT_EQ("q", e.key);
      EXPECT_EQ("Expected integer for `b`, got: \"not a number\"", e.error);
    }
  }
}
