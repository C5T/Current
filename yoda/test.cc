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

#include "api/key_entry/test.cc"
#include "api/matrix/test.cc"

#include "yoda.h"
#include "test_types.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/3party/gtest/gtest-main-with-dflags.h"

TEST(Yoda, CoverTest) {
  typedef yoda::API<YodaTestEntryBase, yoda::KeyEntry<KeyValueEntry>, yoda::KeyEntry<StringKVEntry>> TestAPI;
  TestAPI api("YodaCoverTest");

  api.UnsafeStream().Emplace(new StringKVEntry());

  while (!api.CaughtUp()) {
    ;
  }

  api.Add(KeyValueEntry(1, 42.0));
  EXPECT_EQ(42.0, api.Get(1).value);
  api.Add(StringKVEntry("foo", "bar"));
  EXPECT_EQ("bar", api.Get("foo").foo);
}
