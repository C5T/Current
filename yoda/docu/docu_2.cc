// TODO(dkorolev): Rename `Call` into `Transaction`.

/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../yoda.h"

#include "../../../Bricks/strings/printf.h"

#include "../../../Bricks/net/api/api.h"
#include "../../../Bricks/dflags/dflags.h"
#include "../../../Bricks/3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(yoda_docu_test_port, 8999, "");

using yoda::Padawan;
using yoda::API;
using yoda::Future;
using yoda::KeyEntry;
using yoda::MatrixEntry;
using yoda::EntryWrapper;
using yoda::NonexistentEntryAccessed;
using yoda::KeyNotFoundException;
using yoda::KeyAlreadyExistsException;

using bricks::strings::Printf;
using bricks::strings::FromString;

  // Unique types for keys.
  enum class PRIME : int {};
  enum class FIRST_DIGIT : int {};
  enum class SECOND_DIGIT : int {};
  
  // Serializable class `Prime`.
  struct Prime : Padawan {
    PRIME prime;
    int index;
  
    Prime(const int prime = 0, const int index = 0)
      : prime(static_cast<PRIME>(prime)),
        index(index) {
    }
  
    Prime(const Prime&) = default;
  
    PRIME key() const {
      // The `Key()` method would be unnecessary
      // if the `prime` field is called `key`.
      return prime;
    }
  
    template <typename A>
    void serialize(A& ar) {
      Padawan::serialize(ar);
      ar(cereal::make_nvp("prime", reinterpret_cast<int&>(prime)),
         CEREAL_NVP(index));
    }
  };
  CEREAL_REGISTER_TYPE(Prime);
  
  // Serializable class `PrimeCell`.
  struct PrimeCell : Padawan {
    FIRST_DIGIT row;
    SECOND_DIGIT col;
    int index;
  
    PrimeCell(const int a = 0, const int b = 0, const int index = 0)
      : row(static_cast<FIRST_DIGIT>(a)),
        col(static_cast<SECOND_DIGIT>(b)),
        index(index) {
    }
  
    PrimeCell(const PrimeCell&) = default;
  
    template <typename A>
    void serialize(A& ar) {
      Padawan::serialize(ar);
      ar(cereal::make_nvp("d1", reinterpret_cast<int&>(row)),
         cereal::make_nvp("d2", reinterpret_cast<int&>(col)),
         CEREAL_NVP(index));
    }
  };
  CEREAL_REGISTER_TYPE(PrimeCell);
    
TEST(YodaDocu, Test) {
const int port = FLAGS_yoda_docu_test_port;
bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(42));
HTTP(port).ResetAllHandlers();

  // Define the `api` object.
  typedef API<KeyEntry<Prime>, MatrixEntry<PrimeCell>> PrimesAPI;
  PrimesAPI api("YodaExampleUsage");
  
  // `2` is the first prime.
  // `.Go()` (or `.Wait()`) makes `DimaAdd()` a blocking call.
  api.DimaAdd(Prime(2, 1)).Go();

  // `3` is the second prime.
  // `api.Add()` never throws and silently overwrites.
  api.DimaAdd(Prime(3, 100));
  api.DimaAdd(Prime(3, 2));
  
  // `api.Get()` has multiple signatures, one or more per
  // supported data type. It never throws, and returns a wrapper,
  // that can be casted to both `bool` and the underlying type.
  ASSERT_TRUE(static_cast<bool>(api.DimaGet(static_cast<PRIME>(2)).Go()));
  EXPECT_EQ(1, static_cast<const Prime&>(api.DimaGet(static_cast<PRIME>(2)).Go()).index);
  ASSERT_TRUE(static_cast<bool>(api.DimaGet(static_cast<PRIME>(3)).Go()));
  EXPECT_EQ(2, static_cast<const Prime&>(api.DimaGet(static_cast<PRIME>(3)).Go()).index);
  ASSERT_FALSE(static_cast<bool>(api.DimaGet(static_cast<PRIME>(4)).Go()));
  
  // Expanded syntax for `Add()`.
{
  api.Call([](PrimesAPI::T_DATA data) {
    KeyEntry<Prime>::Mutator(data).Add(Prime(5, 3));
  }).Wait();
  
  api.Call([](PrimesAPI::T_DATA data) {
    KeyEntry<Prime>::Mutator(data).Add(Prime(7, 100));
  }).Wait();
  
  // `Add()`: Overwrite is OK.
  api.Call([](PrimesAPI::T_DATA data) {
    KeyEntry<Prime>::Mutator(data).Add(Prime(7, 4));
  }).Wait();
}
    
  // Expanded syntax for `Get()`.
{
  Future<EntryWrapper<Prime>> future1 = api.Call([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(2));
  });
  EntryWrapper<Prime> entry1 = future1.Go();
  
  const bool b1 = entry1;
  ASSERT_TRUE(b1);
  
  const Prime& p1 = entry1;
  EXPECT_EQ(1, p1.index);
  
  Future<EntryWrapper<Prime>> future2 = api.Call([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(5));
  });
  EntryWrapper<Prime> entry2 = future2.Go();
  
  const bool b2 = entry2;
  ASSERT_TRUE(b2);
  
  const Prime& p2 = entry2;
  EXPECT_EQ(3, p2.index);
  
  Future<EntryWrapper<Prime>> future3 = api.Call([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(7));
  });
  EntryWrapper<Prime> entry3 = future3.Go();
  
  const bool b3 = entry3;
  ASSERT_TRUE(b3);
  
  const Prime& p3 = entry3;
  EXPECT_EQ(4, p3.index);
  
  Future<EntryWrapper<Prime>> future4 = api.Call([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(8));
  });
  EntryWrapper<Prime> entry4 = future4.Go();
  
  const bool b4 = entry4;
  ASSERT_FALSE(b4);
}
  
  // Accessing the memory view of `data`.
{
  api.Call([](PrimesAPI::T_DATA data) {
    auto adder = KeyEntry<Prime>::Mutator(data);
    const auto getter = KeyEntry<Prime>::Accessor(data);
  
    // `adder.Add()` in a non-throwing call.
    adder.Add(Prime(11, 5));
    adder.Add(Prime(13, 100));
    adder.Add(Prime(13, 6));  // Overwrite.

    // `adder.operator<<()` is a potentially throwing call.
    adder << Prime(17, 7) << Prime(19, 9);
    ASSERT_THROW(adder << Prime(19, 9), KeyAlreadyExistsException<PRIME>);
    try {
      adder << Prime(19, 9);
    } catch (const KeyAlreadyExistsException<PRIME>& e) {
      EXPECT_EQ(19, static_cast<int>(e.key));
    }

    // `getter.Get()` in a non-throwing call, returning a wrapper.
    const auto p13 = getter.Get(static_cast<PRIME>(13));
    ASSERT_TRUE(static_cast<bool>(p13));
    EXPECT_EQ(6, static_cast<const Prime&>(p13).index);
  
    // `getter.operator[]()` is a potentially throwing call, returning a value.
    EXPECT_EQ(3, getter[static_cast<PRIME>(5)].index);
    EXPECT_EQ(7, getter[static_cast<PRIME>(17)].index);
  
    // Query a non-existing value using two ways.
    const auto p8 = getter.Get(static_cast<PRIME>(8));
    ASSERT_FALSE(static_cast<bool>(p8));
    ASSERT_THROW(static_cast<void>(static_cast<const Prime&>(p8)),
                 NonexistentEntryAccessed);
      
    ASSERT_THROW(getter[static_cast<PRIME>(9)],
                 KeyNotFoundException<PRIME>);
    try {
      getter[static_cast<PRIME>(9)];
    } catch(const KeyNotFoundException<PRIME>& e) {
      EXPECT_EQ(9, static_cast<int>(e.key));
    }
  
    // The syntax using `data` directly, without `Accessor` or `Mutator`.
    data << Prime(23, 10) << Prime(29, 101);
    ASSERT_THROW(data << Prime(29, 102),
                 KeyAlreadyExistsException<PRIME>);
    data.Add(Prime(29, 11));
    ASSERT_TRUE(static_cast<bool>(data.Get(static_cast<PRIME>(3))));
    EXPECT_EQ(2, static_cast<const Prime&>(data.Get(static_cast<PRIME>(3))).index);
    ASSERT_FALSE(static_cast<bool>(data.Get(static_cast<PRIME>(4))));
    EXPECT_EQ(3, data[static_cast<PRIME>(5)].index);
    ASSERT_THROW(data[static_cast<PRIME>(9)],
                 KeyNotFoundException<PRIME>);
  
    // Traversal.
    EXPECT_EQ(10u, getter.size());
    EXPECT_EQ(10u, adder.size());
  
    std::set<std::pair<int, int>> as_set;  // To ensure the order is right.
    for (auto cit = getter.begin(); cit != getter.end(); ++cit) {
      as_set.insert(std::make_pair((*cit).index, static_cast<int>(cit->prime)));
    }
    std::ostringstream os;
    for (const auto cit : as_set) {
      os << ',' << cit.first << ':' << cit.second;
    }
    EXPECT_EQ("1:2,2:3,3:5,4:7,5:11,6:13,7:17,9:19,10:23,11:29",
              os.str().substr(1));
      
    size_t c1 = 0u;
    size_t c2 = 0u;
    for (const auto cit : getter) {
      ++c1;
    }
    for (const auto cit : adder) {
      ++c2;
    }
    EXPECT_EQ(10u, c1);
    EXPECT_EQ(10u, c2);
  }).Go();
  
  // Work with `MatrixEntry<>` as well.
  api.DimaAdd(PrimeCell(0, 2, 1));
}
  
  // The return value from `Call()` is wrapped into a `Future<>`,
  // use `.Go()` to retrieve the result.
  // (Or `.Wait()` to just wait for the passed in function to complete.)
{
  Future<std::string> future = api.Call([](PrimesAPI::T_DATA data) {
    const auto getter = KeyEntry<Prime>::Accessor(data);
    return Printf("[2]=%d,[3]=%d,[5]*[7]=%d",
                  getter[static_cast<PRIME>(2)].index, 
                  getter[static_cast<PRIME>(3)].index,
                  getter[static_cast<PRIME>(5)].index *
                  getter[static_cast<PRIME>(7)].index);
  });
  EXPECT_EQ("[2]=1,[3]=2,[5]*[7]=12", future.Go());
}
  
{
  // Confirm that the send parameter callback is `std::move()`-d
  // into the processing thread.
  // Interestingly, a REST-ful endpoint is the easiest possible test.
  HTTP(port).Register("/rest", [&api](Request request) {
    api.DimaGet2(static_cast<PRIME>(FromString<int>(request.url.query["p"])),
                 std::move(request));
  });
  auto response_prime = HTTP(GET(Printf("http://localhost:%d/rest?p=7", port)));
  EXPECT_EQ(200, static_cast<int>(response_prime.code));
  EXPECT_EQ("{\"entry\":{\"ms\":42,\"prime\":7,\"index\":4}}\n", response_prime.body);
  auto response_composite = HTTP(GET(Printf("http://localhost:%d/rest?p=9", port)));
  EXPECT_EQ(404, static_cast<int>(response_composite.code));
  EXPECT_EQ("{\"error\":\"NOT_FOUND\"}\n", response_composite.body);
}
  
{
  // Confirm that the stream is indeed populated.
  api.ExposeViaHTTP(port, "/data");
  EXPECT_EQ(
#if 1
"{\"entry\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"Prime\",\"ptr_wrapper\":{\"valid\":1,\"data\":{\"ms\":42,\"prime\":2,\"index\":1}}}}\n",
#else
    "... JSON represenation of the first entry ...",
#endif
    HTTP(GET(Printf("http://localhost:%d/data?cap=1", port))).body);
  EXPECT_EQ(
#if 1
"{\"entry\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"PrimeCell\",\"ptr_wrapper\":{\"valid\":1,\"data\":{\"ms\":42,\"d1\":0,\"d2\":2,\"index\":1}}}}\n",
#else
    "... JSON represenation of the last entry ...",
#endif
    HTTP(GET(Printf("http://localhost:%d/data?n=1", port))).body);
}
  
  // Prime p = api.AsyncGet(static_cast<PRIME>(2)).Go();
  // EXPECT_EQ(1, p.index);

  // DIMA: Show implicit casts.
  // DIMA: Show promises.

#if 0
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
  std::atomic_bool done(false);
  api.Call([&](DemoAPI::T_DATA data) {
    auto KVEntries = KeyEntry<KeyValueEntry>::Accessor(data);

    EXPECT_TRUE(KVEntries.Exists(1));
    EXPECT_FALSE(KVEntries.Exists(100500));

    // `Get()` syntax.
    EXPECT_EQ(42.0, static_cast<const KeyValueEntry&>(KVEntries.Get(1)).value);
    EXPECT_FALSE(KVEntries.Get(56));

    // `operator[]` syntax.
    EXPECT_EQ(42.0, static_cast<const KeyValueEntry&>(KVEntries[1]).value);
    ASSERT_THROW(static_cast<void>(static_cast<const KeyValueEntry&>(KVEntries[34]).value),
                 yoda::KeyNotFoundCoverException);

    auto mutable_kve = KeyEntry<KeyValueEntry>::Mutator(data);
    mutable_kve.Add(KeyValueEntry(128, 512.0));
    EXPECT_EQ(512.0, static_cast<const KeyValueEntry&>(mutable_kve[128]).value);

    auto mutable_matrix = MatrixEntry<MatrixCell>::Mutator(data);
    EXPECT_EQ(2, static_cast<const MatrixCell&>(mutable_matrix.Get(1, "test")).value);
    /*
        const bool exists = data.Get(1);
        EXPECT_TRUE(exists);
        yoda::EntryWrapper<KeyValueEntry> entry = data.Get(1);
        EXPECT_EQ(42.0, entry().value);
        EXPECT_TRUE(data.Get(42, "answer"));
        EXPECT_EQ(100, data.Get(42, "answer")().value);
        EXPECT_TRUE(data.Get("foo"));
        EXPECT_EQ("bar", data.Get("foo")().foo);

        EXPECT_FALSE(data.Get(-1));
        EXPECT_FALSE(data.Get(41, "not an answer"));
        EXPECT_FALSE(data.Get("bazinga"));

        // Accessing nonexistent entry throws an exception.
        ASSERT_THROW(data.Get(1000)(), yoda::NonexistentEntryAccessed);

        double result = 0.0;
        for (int i = 1; i <= 3; ++i) {
          result += data.Get(i)().value * static_cast<double>(data.Get(i, "test")().value);
        }
        data.Add(StringKVEntry("result", Printf("%.2f", result)));
        data.Add(MatrixCell(123, "test", 11));
        data.Add(KeyValueEntry(42, 1.23));
    */
    done = true;
  });

  HTTP(FLAGS_yoda_docu_test_port).ResetAllHandlers();
  api.ExposeViaHTTP(FLAGS_yoda_docu_test_port, "/data");
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
            HTTP(GET(Printf("http://localhost:%d/data?cap=9", FLAGS_yoda_docu_test_port))).body);

  while (!done || !api.CaughtUp()) {
    ;  // Spin lock.
  }
  /*
    struct HappyEnding {
      Request request;
      explicit HappyEnding(Request request) : request(std::move(request)) {}
      void operator()(double result) { request(Printf("Magic: %.3lf", result)); }
    };

    HTTP(FLAGS_yoda_docu_test_port).ResetAllHandlers();
    HTTP(FLAGS_yoda_docu_test_port)
        .Register("/omfg", [&api](Request r) {
          // Note: C standard does not regulate the order in which parameters are evaluated,
          // thus, if any data should be extracted from `r.query`, it should be done before the next line
          // since it does `std::move(r)`.
          api.Call([](DemoAPI::T_DATA& data) { return data.Get(2)().value * data.Get(3)().value; },
                   HappyEnding(std::move(r)));
        });
    const auto response = HTTP(GET(Printf("http://localhost:%d/omfg", FLAGS_yoda_docu_test_port)));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("Magic: 352.800", response.body);

    EXPECT_EQ("160.30", api.Get("result").foo);
    EXPECT_EQ(11, api.Get(123, "test").value);
    EXPECT_EQ(1.23, api.Get(42).value);
  */
#endif
}
