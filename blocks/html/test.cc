/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#define CURRENT_HTML_UNIT_TEST

#include "html.h"
#include "html_http.h"

#include <thread>

#include "../http/api.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

TEST(HTMLTest, Smoke) {
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(b);
      HTML(_) << "Bold";
    }
    EXPECT_EQ("<b>Bold</b>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(i);
      HTML(_) << "Italic";
    }
    EXPECT_EQ("<i>Italic</i>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(pre);
      HTML(_) << "Monospace";
    }
    EXPECT_EQ("<pre>Monospace</pre>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(_) << "line";
      HTML(br);
      HTML(_) << "break";
    }
    EXPECT_EQ("line<br>break", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(a, href("https://github.com/C5T/Current"));
      HTML(_) << "Duh.";
    }
    EXPECT_EQ("<a href='https://github.com/C5T/Current'>Duh.</a>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(a, href("https://github.com/C5T/Current"));
      {
        HTML(i);
        HTML(_) << "This";
      }
      HTML(_) << ' ';
      {
        HTML(p);
        HTML(_) << "is";
      }
      HTML(_) << ' ';
      {
        HTML(b);
        HTML(_) << "Sparta!";
      }
    }
    EXPECT_EQ("<a href='https://github.com/C5T/Current'><i>This</i> <p>is</p> <b>Sparta!</b></a>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(form, method("GET").action("http://localhost"));
      HTML(input, type("submit").value("Ha!"));
    }
    EXPECT_EQ("<form method='GET' action='http://localhost'><input type='submit' value='Ha!'></form>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      {
        HTML(table);
        for (size_t i = 0; i < 2; ++i) {
          HTML(tr);
          for (size_t j = 0; j < 2; ++j) {
            HTML(td);
            HTML(_) << i + 1 << j + 1;
          }
        }
      }
    }
    EXPECT_EQ("<table><tr><td>11</td><td>12</td></tr><tr><td>21</td><td>22</td></tr></table>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(td, colspan("42").align("test").valign("passed"));
    }
    EXPECT_EQ("<td align='test' colspan='42' valign='passed'></td>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(table, border(0));  // Number, not a string.
    }
    EXPECT_EQ("<table border='0'></table>", oss.str());
  }
  {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(input, value(42));  // Number, not a string.
    }
    EXPECT_EQ("<input value='42'>", oss.str());
  }
}

TEST(HTMLTest, ScopeSanityChecks) {
  {
    std::ostringstream oss;
    ASSERT_FALSE(
        ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().HasActiveScope());
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      ASSERT_TRUE(
          ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().HasActiveScope());
      HTML(_) << "Hello, World!";
    }
    ASSERT_FALSE(
        ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().HasActiveScope());
    EXPECT_EQ("Hello, World!", oss.str());
  }
  {
    std::ostringstream oss;
    ASSERT_FALSE(
        ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().HasActiveScope());
    auto scope = std::make_unique<current::html::HTMLGeneratorOStreamScope>(oss);
    ASSERT_TRUE(::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().HasActiveScope());
    HTML(_) << "And once again.";
    scope = nullptr;
    ASSERT_FALSE(
        ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().HasActiveScope());
    EXPECT_EQ("And once again.", oss.str());
  }
}

TEST(HTMLTest, OutsideScopeException) {
  int expected_line = -1;
  try {
    expected_line = __LINE__ + 1;
    HTML(b);
    HTML(_) << "The above line should fail.";
    ASSERT_TRUE(false);
  } catch (const current::html::HTMLGenerationNoActiveScopeException& e) {
    EXPECT_STREQ("b", e.tag);
    std::string file = e.file;
    std::string expected_file_suffix = "test.cc";
    ASSERT_GE(file.length(), expected_file_suffix.length());
    EXPECT_EQ(expected_file_suffix, file.substr(file.length() - expected_file_suffix.length()));  // Strip the path.
    EXPECT_EQ(expected_line, e.line);
  }
}

TEST(HTMLTest, IntersectingScopesException) {
  std::ostringstream oss1;
  std::ostringstream oss2;
  {
    const auto scope1 = current::html::HTMLGeneratorOStreamScope(oss1);
    HTML(_) << "scope one\n";
    try {
      const auto scope2 = current::html::HTMLGeneratorOStreamScope(oss2);
      HTML(_) << "scope two won't really start\n";
      ASSERT_TRUE(false);
    } catch (const current::html::HTMLGenerationIntersectingScopesException&) {
      HTML(_) << "ok, no intersecting scopes\n";
    }
    HTML(_) << "done\n";
  }
  EXPECT_EQ("scope one\nok, no intersecting scopes\ndone\n", oss1.str());
  EXPECT_EQ("", oss2.str());
}

TEST(HTMLTest, ThreadIsolation) {
  std::string r1;
  std::string r2;
  std::thread t1([&r1]() {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(_) << "Thread ";
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      HTML(_) << "one";
    }
    r1 = oss.str();
  });
  std::thread t2([&r2]() {
    std::ostringstream oss;
    {
      const auto scope = current::html::HTMLGeneratorOStreamScope(oss);
      HTML(_) << "Thread ";
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      HTML(_) << "two";
    }
    r2 = oss.str();
  });
  t1.join();
  t2.join();
  EXPECT_EQ("Thread one", r1);
  EXPECT_EQ("Thread two", r2);
}

TEST(HTMLTest, HTTPIntegration) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  const auto http_route_scope = HTTP(std::move(reserved_port)).Register("/", [](Request r) {
    const current::html::HTMLGeneratorHTTPResponseScope html_scope(std::move(r));
    HTML(html);
    {
      HTML(head);
      HTML(title);
      HTML(_) << "Yo!";
    }
    {
      HTML(body);
      {
        HTML(i);
        HTML(_) << "Test";
      }
      HTML(_) << ' ';
      {
        HTML(font, color("green").size("+1"));
        HTML(b);
        HTML(_) << "PASSED";
      }
      HTML(_) << '.';
    }
  });
  const std::string url = Printf("http://localhost:%d/", port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "<html><head><title>Yo!</title></head>"
      "<body><i>Test</i> <font color='green' size='+1'><b>PASSED</b></font>.</body></html>",
      response.body);
  EXPECT_EQ("text/html; charset=utf-8", response.headers.GetOrDefault("Content-Type", "N/A"));
}
