/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_COVERAGE_REPORT_MODE

#include "../../Sherlock/sherlock.h"

#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace type_test {

#include "include/sherlock.h"

}  // namespace type_test

TEST(TypeTest, Sherlock) {
  using namespace type_test;

  using stream_t = current::sherlock::Stream<stream_variant_t, current::persistence::File>;

  const auto persistence_file_remover = current::FileSystem::ScopedRmFile("data");

  stream_t stream("data");

#include "include/sherlock.cc"

  EXPECT_EQ(entries_count, stream.Persister().Size());
}

#endif  // CURRENT_COVERAGE_REPORT_MODE
