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

#include "impl.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#ifndef CURRENT_COVERAGE_REPORT_MODE
static const char* const kDefaultPathToIrisDataset = "../iris/data/dataset.json";
#else
static const char* const kDefaultPathToIrisDataset = "golden/dataset.json";
#endif
DEFINE_string(dlopen_example_test_input_filename, kDefaultPathToIrisDataset, "The input Irises dataset.");
DEFINE_uint16(dlopen_example_test_port, PickPortForUnitTest(), "");

#ifndef CURRENT_WINDOWS
TEST(DLOpenExample, Smoke) {
  DynamicDLOpenIrisExampleImpl impl(FLAGS_dlopen_example_test_input_filename, FLAGS_dlopen_example_test_port);

  const int port = static_cast<int>(FLAGS_dlopen_example_test_port);

  EXPECT_EQ(
      // clang-format off
      "#include <vector>\n"
      "#include <sstream>\n"
      "#include <iomanip>\n"
      "\n"
      "struct Base {\n"
      "  virtual ~Base() {}\n"
      "};\n"
      "\n"
      "struct Flower : Base {\n"
      "  virtual ~Flower() {}\n"
      "  union {\n"
      "    struct {\n"
      "      double sl;\n"
      "      double sw;\n"
      "      double pl;\n"
      "      double pw;\n"
      "    };\n"
      "    double x[4];\n"
      "  };\n"
      "  std::string label;\n"
      "};\n"
      "\n"
      "extern \"C\" void Run(const std::vector<Flower>& flowers, std::ostringstream& os) {\n"
      "### USER CODE ###\n"
      "}\n",
      // clang-format on
      HTTP(GET(current::strings::Printf("http://localhost:%d/boilerplate", port))).body);

  {
    const auto response = HTTP(GET(current::strings::Printf("http://localhost:%d/", port)));
    EXPECT_EQ(405, static_cast<int>(response.code));
    EXPECT_EQ("POST a piece of code onto `/` or GET `/${SHA256}` to execute it.\n", response.body);
    EXPECT_EQ("0", response.headers.Get("X-Total"));
  }

  {
    const auto response = HTTP(GET(current::strings::Printf("http://localhost:%d/blah", port)));
    EXPECT_EQ(404, static_cast<int>(response.code));
    EXPECT_EQ("No compiled code with this SHA256 was found.\n", response.body);
  }

  {
    const std::string f_simple = "os << \"Test passed!\" << std::endl;";
    ASSERT_EQ("e17377dd4a946033f78d952b0f6eb6f2c9e3b766a85a6cfbae25656cfb236f5a", current::SHA256(f_simple));
    {
      const auto post_response = HTTP(POST(current::strings::Printf("http://localhost:%d/", port), f_simple));
      EXPECT_EQ(201, static_cast<int>(post_response.code));
      EXPECT_EQ("Compiled: @" + current::SHA256(f_simple) + '\n', post_response.body);
      EXPECT_EQ(current::SHA256(f_simple), post_response.headers.Get("X-SHA"));
    }
    {
      const auto get_response =
          HTTP(GET(current::strings::Printf("http://localhost:%d/", port) + current::SHA256(f_simple)));
      EXPECT_EQ(200, static_cast<int>(get_response.code));
      EXPECT_EQ("Test passed!\n", get_response.body);
      EXPECT_TRUE(get_response.headers.Has("X-MS"));
      EXPECT_TRUE(get_response.headers.Has("X-QPS"));
    }
  }

  EXPECT_EQ("1", HTTP(GET(current::strings::Printf("http://localhost:%d/", port))).headers.Get("X-Total"));

  {
    const std::string f_bs = "*** -> this thing won't compile ***";
    const auto post_response = HTTP(POST(current::strings::Printf("http://localhost:%d/", port), f_bs));
    EXPECT_EQ(400, static_cast<int>(post_response.code));
    EXPECT_FALSE(post_response.headers.Has("X-SHA"));
  }

  EXPECT_EQ("1", HTTP(GET(current::strings::Printf("http://localhost:%d/", port))).headers.Get("X-Total"));

  {
    // Just test data access doesn't SEGFAULT.
    const std::string f_with_data_access =
        "for (int i = 0; i < flowers.size(); i += 17) os << (i ? \",\" : \"\") << flowers[i].label;";
    ASSERT_EQ("589b1ed258d0f1b4634ab64ff7eba7ad942ae11f3663fb4ddcd21ae0946e9638", current::SHA256(f_with_data_access));
    {
      const auto post_response = HTTP(POST(current::strings::Printf("http://localhost:%d/", port), f_with_data_access));
      EXPECT_EQ(201, static_cast<int>(post_response.code));
      EXPECT_EQ("Compiled: @" + current::SHA256(f_with_data_access) + '\n', post_response.body);
      EXPECT_EQ(current::SHA256(f_with_data_access), post_response.headers.Get("X-SHA"));
    }
    {
      const auto get_response =
          HTTP(GET(current::strings::Printf("http://localhost:%d/", port) + current::SHA256(f_with_data_access)));
      EXPECT_EQ(200, static_cast<int>(get_response.code));
      EXPECT_EQ("setosa,setosa,setosa,versicolor,versicolor,versicolor,virginica,virginica,virginica",
                get_response.body);
      EXPECT_TRUE(get_response.headers.Has("X-MS"));
      EXPECT_TRUE(get_response.headers.Has("X-QPS"));
    }
  }

  EXPECT_EQ("2", HTTP(GET(current::strings::Printf("http://localhost:%d/", port))).headers.Get("X-Total"));
}
#endif  // CURRENT_WINDOWS