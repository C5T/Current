/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#define BRICKS_MOCK_TIME

#include "api/key_entry/test.cc"
#include "api/matrix/test.cc"

#include "yoda.h"
#include "test_types.h"

#include "../../Bricks/net/api/api.h"
#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/3party/gtest/gtest-main-with-dflags.h"
#include "../../Bricks/strings/printf.h"

DEFINE_int32(yoda_test_port, 8991, "");

using bricks::strings::Printf;

TEST(Yoda, CoverTest) {
  using yoda::API;
  using yoda::KeyEntry;
  using yoda::MatrixEntry;
  typedef API<KeyEntry<KeyValueEntry>, MatrixEntry<MatrixCell>, KeyEntry<StringKVEntry>> TestAPI;
  TestAPI api("YodaCoverTest");

  api.Add(KeyValueEntry(1, 42.0));
  EXPECT_EQ(42.0, api.Get(1).value);
  api.Add(MatrixCell(42, "answer", 100));
  EXPECT_EQ(100, api.Get(42, "answer").value);
  api.Add(StringKVEntry("foo", "bar"));
  EXPECT_EQ("bar", api.Get("foo").foo);

  // Adding some more values.
  api.Add(KeyValueEntry(2, 31.5));
  api.Add(KeyValueEntry(3, 11.5));
  api.Add(MatrixCell(1, "test", 2));
  api.Add(MatrixCell(2, "test", 1));
  api.Add(MatrixCell(3, "test", 4));

  // Asynchronous call of user function.
  bool done = false;
  api.Call([&](TestAPI::T_CONTAINER_WRAPPER cw) {
    auto KVEntries = KeyEntry<KeyValueEntry>::Accessor(cw);

    EXPECT_TRUE(KVEntries.Exists(1));
    EXPECT_FALSE(KVEntries.Exists(100500));

    // `Get()` syntax.
    EXPECT_EQ(42.0, static_cast<const KeyValueEntry&>(KVEntries.Get(1)).value);
    EXPECT_FALSE(KVEntries.Get(56));

    // `operator[]` syntax.
    EXPECT_EQ(42.0, static_cast<const KeyValueEntry&>(KVEntries[1]).value);
    KeyValueEntry kve34;
    ASSERT_THROW(kve34 = KVEntries[34], yoda::KeyNotFoundCoverException);

    auto mutable_kve = KeyEntry<KeyValueEntry>::Mutator(cw);
    mutable_kve.Add(KeyValueEntry(128, 512.0));
    EXPECT_EQ(512.0, static_cast<const KeyValueEntry&>(mutable_kve[128]).value);

    auto mutable_matrix = MatrixEntry<MatrixCell>::Mutator(cw);
    EXPECT_EQ(2, static_cast<const MatrixCell&>(mutable_matrix.Get(1, "test")).value);
    /*
        const bool exists = cw.Get(1);
        EXPECT_TRUE(exists);
        yoda::EntryWrapper<KeyValueEntry> entry = cw.Get(1);
        EXPECT_EQ(42.0, entry().value);
        EXPECT_TRUE(cw.Get(42, "answer"));
        EXPECT_EQ(100, cw.Get(42, "answer")().value);
        EXPECT_TRUE(cw.Get("foo"));
        EXPECT_EQ("bar", cw.Get("foo")().foo);

        EXPECT_FALSE(cw.Get(-1));
        EXPECT_FALSE(cw.Get(41, "not an answer"));
        EXPECT_FALSE(cw.Get("bazinga"));

        // Accessing nonexistent entry throws an exception.
        ASSERT_THROW(cw.Get(1000)(), yoda::NonexistentEntryAccessed);

        double result = 0.0;
        for (int i = 1; i <= 3; ++i) {
          result += cw.Get(i)().value * static_cast<double>(cw.Get(i, "test")().value);
        }
        cw.Add(StringKVEntry("result", Printf("%.2f", result)));
        cw.Add(MatrixCell(123, "test", 11));
        cw.Add(KeyValueEntry(42, 1.23));
    */
    done = true;
  });

  HTTP(FLAGS_yoda_test_port).ResetAllHandlers();
  api.ExposeViaHTTP(FLAGS_yoda_test_port, "/data");
  const std::string Z = "";  // For `clang-format`-indentation purposes.
  EXPECT_EQ(Z + JSON(WithBaseType<Padawan>(KeyValueEntry(1, 42)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(42, "answer", 100)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(StringKVEntry("foo", "bar")), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(2, 31.5)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(3, 11.5)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(1, "test", 2)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(2, "test", 1)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(3, "test", 4)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(128, 512)), "entry") + '\n',
            HTTP(GET(Printf("http://localhost:%d/data?cap=9", FLAGS_yoda_test_port))).body);

  while (!done || !api.CaughtUp()) {
    ;  // Spin lock.
  }
  /*
    struct HappyEnding {
      Request request;
      explicit HappyEnding(Request request) : request(std::move(request)) {}
      void operator()(double result) { request(Printf("Magic: %.3lf", result)); }
    };

    HTTP(FLAGS_yoda_test_port).ResetAllHandlers();
    HTTP(FLAGS_yoda_test_port)
        .Register("/omfg", [&api](Request r) {
          // Note: C standard does not regulate the order in which parameters are evaluated,
          // thus, if any data should be extracted from `r.query`, it should be done before the next line
          // since it does `std::move(r)`.
          api.Call([](TestAPI::T_CONTAINER_WRAPPER& cw) { return cw.Get(2)().value * cw.Get(3)().value; },
                   HappyEnding(std::move(r)));
        });
    const auto response = HTTP(GET(Printf("http://localhost:%d/omfg", FLAGS_yoda_test_port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("Magic: 352.800", response.body);

    EXPECT_EQ("160.30", api.Get("result").foo);
    EXPECT_EQ(11, api.Get(123, "test").value);
    EXPECT_EQ(1.23, api.Get(42).value);
  */
}
