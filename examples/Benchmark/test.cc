/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "server.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(benchmark_test_local_port, 8765, "The local port to spawn test server on.");

TEST(BenchmarkTest, OneAndOne) {
  BenchmarkTestServer server(FLAGS_benchmark_test_local_port, "/add");
  const auto response = HTTP(GET(Printf("http://localhost:%d/add?a=1&b=1", FLAGS_benchmark_test_local_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(2, ParseJSON<AddResult>(response.body).sum);
}

TEST(BenchmarkTest, TenAndTen) {
  BenchmarkTestServer server(FLAGS_benchmark_test_local_port, "/add");
  const auto response = HTTP(GET(Printf("http://localhost:%d/add?a=10&b=10", FLAGS_benchmark_test_local_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(20, ParseJSON<AddResult>(response.body).sum);
}
