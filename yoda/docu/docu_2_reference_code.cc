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
using yoda::CellNotFoundException;
using yoda::CellAlreadyExistsException;

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
  // `.Go()` (or `.Wait()`) makes `Add()` a blocking call.
  api.Add(Prime(2, 1)).Go();

  // `3` is the second prime.
  // `api.Add()` never throws and silently overwrites.
  api.Add(Prime(3, 100));
  api.Add(Prime(3, 2));
  
  // `api.Get()` has multiple signatures, one or more per
  // supported data type. It never throws, and returns a wrapper,
  // that can be casted to both `bool` and the underlying type.
  ASSERT_TRUE(static_cast<bool>(api.Get(static_cast<PRIME>(2)).Go()));
  ASSERT_TRUE(static_cast<bool>(api.Get(std::make_tuple(static_cast<PRIME>(2))).Go()));
  EXPECT_EQ(1, static_cast<const Prime&>(api.Get(static_cast<PRIME>(2)).Go()).index);
  ASSERT_TRUE(static_cast<bool>(api.Get(static_cast<PRIME>(3)).Go()));
  EXPECT_EQ(2, static_cast<const Prime&>(api.Get(static_cast<PRIME>(3)).Go()).index);
  ASSERT_FALSE(static_cast<bool>(api.Get(static_cast<PRIME>(4)).Go()));
  
  // Expanded syntax for `Add()`.
{
  api.Transaction([](PrimesAPI::T_DATA data) {
    KeyEntry<Prime>::Mutator(data).Add(Prime(5, 3));
  }).Wait();
  
  api.Transaction([](PrimesAPI::T_DATA data) {
    KeyEntry<Prime>::Mutator(data).Add(Prime(7, 100));
  }).Wait();
  
  // `Add()`: Overwrite is OK.
  api.Transaction([](PrimesAPI::T_DATA data) {
    KeyEntry<Prime>::Mutator(data).Add(Prime(7, 4));
  }).Wait();
}
    
  // Expanded syntax for `Get()`.
{
  Future<EntryWrapper<Prime>> future2 = api.Transaction([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(2));
  });
  EntryWrapper<Prime> entry2 = future2.Go();
  
  const bool b2 = entry2;
  ASSERT_TRUE(b2);
  
  const Prime& p2 = entry2;
  EXPECT_EQ(1, p2.index);
  
  Future<EntryWrapper<Prime>> future5 = api.Transaction([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(5));
  });
  EntryWrapper<Prime> entry5 = future5.Go();
  
  const bool b5 = entry5;
  ASSERT_TRUE(b5);
  
  const Prime& p5 = entry5;
  EXPECT_EQ(3, p5.index);
  
  Future<EntryWrapper<Prime>> future7 = api.Transaction([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(7));
  });
  EntryWrapper<Prime> entry7 = future7.Go();
  
  const bool b7 = entry7;
  ASSERT_TRUE(b7);
  
  const Prime& p7 = entry7;
  EXPECT_EQ(4, p7.index);
  
  Future<EntryWrapper<Prime>> future8 = api.Transaction([](PrimesAPI::T_DATA data) {
    return KeyEntry<Prime>::Accessor(data).Get(static_cast<PRIME>(8));
  });
  EntryWrapper<Prime> entry8 = future8.Go();
  
  const bool b8 = entry8;
  ASSERT_FALSE(b8);
}
  
  // Accessing the memory view of `data`.
{
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto getter = KeyEntry<Prime>::Accessor(data);
    auto adder = KeyEntry<Prime>::Mutator(data);
  
    // `adder.Add()` in a non-throwing call.
    adder.Add(Prime(11, 5));
    adder.Add(Prime(13, 100));
    adder.Add(Prime(13, 6));  // Overwrite.
    adder.Add(std::tuple<Prime>(Prime(13, 6)));  // Again.

    // `adder.operator<<()` is a potentially throwing call.
    adder << Prime(17, 7) << Prime(19, 9);
    ASSERT_THROW(adder << Prime(19, 9), KeyAlreadyExistsException<Prime>);
    try {
      data << Prime(19, 9);
    } catch (const KeyAlreadyExistsException<Prime>& e) {
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
                 KeyNotFoundException<Prime>);
    try {
      getter[static_cast<PRIME>(9)];
    } catch(const KeyNotFoundException<Prime>& e) {
      EXPECT_EQ(9, static_cast<int>(e.key));
    }
  
    // The syntax using `data` directly, without `Accessor` or `Mutator`.
    data << Prime(23, 10) << Prime(29, 101);
    ASSERT_THROW(data << Prime(29, 102),
                 KeyAlreadyExistsException<Prime>);
    data.Add(Prime(29, 11));
    ASSERT_TRUE(static_cast<bool>(data.Get(std::make_tuple(static_cast<PRIME>(3)))));
    ASSERT_TRUE(static_cast<bool>(data.Get(static_cast<PRIME>(3))));
    EXPECT_EQ(2, static_cast<const Prime&>(data.Get(static_cast<PRIME>(3))).index);
    ASSERT_FALSE(static_cast<bool>(data.Get(static_cast<PRIME>(4))));
    EXPECT_EQ(3, data[static_cast<PRIME>(5)].index);
    ASSERT_THROW(data[static_cast<PRIME>(9)],
                 KeyNotFoundException<Prime>);
  
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
}
  
{
  // Work with `MatrixEntry<>` as well.
  api.Add(PrimeCell(0, 2, 100));
  api.Add(PrimeCell(0, 2, 1));  // Overwrite.
  api.Add(PrimeCell(0, 3, 2));
  
  const EntryWrapper<PrimeCell> e2 =
    api.Get(static_cast<FIRST_DIGIT>(0), static_cast<SECOND_DIGIT>(2)).Go();
  const bool b2 = e2;
  ASSERT_TRUE(b2);
  const PrimeCell p2 = e2;
  EXPECT_EQ(0, static_cast<int>(p2.row));
  EXPECT_EQ(2, static_cast<int>(p2.col));
  EXPECT_EQ(1, p2.index);
   
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto getter = MatrixEntry<PrimeCell>::Accessor(data);
    auto adder = MatrixEntry<PrimeCell>::Mutator(data);
  
    const EntryWrapper<PrimeCell> e3 =
      getter.Get(static_cast<FIRST_DIGIT>(0), static_cast<SECOND_DIGIT>(3));
    const bool b3 = e3;
    ASSERT_TRUE(b3);
    const PrimeCell p3 = e3;
    EXPECT_EQ(0, static_cast<int>(p3.row));
    EXPECT_EQ(3, static_cast<int>(p3.col));
    EXPECT_EQ(2, p3.index);
  
    const EntryWrapper<PrimeCell> e4 =
      getter.Get(static_cast<FIRST_DIGIT>(0), static_cast<SECOND_DIGIT>(4));
    const bool b4 = e4;
    ASSERT_FALSE(b4);
    ASSERT_THROW(static_cast<void>(static_cast<const PrimeCell&>(e4)),
                 NonexistentEntryAccessed);
  
    adder.Add(PrimeCell(0, 5, 3));
    adder.Add(std::tuple<PrimeCell>(PrimeCell(0, 5, 3)));
  
    data.Add(PrimeCell(0, 7, 4));
  
    adder << PrimeCell(1, 1, 5) << PrimeCell(1, 3, 6);
    data << PrimeCell(1, 7, 7);
    ASSERT_THROW(data << PrimeCell(1, 7, 7),
                 CellAlreadyExistsException<PrimeCell>);
  
    EXPECT_EQ(7, getter[std::make_tuple(static_cast<FIRST_DIGIT>(1),
                                        static_cast<SECOND_DIGIT>(7))].index);
  
    ASSERT_THROW(getter[std::make_tuple(static_cast<FIRST_DIGIT>(0),
                                        static_cast<SECOND_DIGIT>(4))],
                 CellNotFoundException<PrimeCell>);
  
    EXPECT_EQ(7, getter[static_cast<FIRST_DIGIT>(1)]
                       [static_cast<SECOND_DIGIT>(7)].index);
  
    EXPECT_EQ(7, getter[static_cast<SECOND_DIGIT>(7)]
                       [static_cast<FIRST_DIGIT>(1)].index);
  
    EXPECT_EQ(7, adder[static_cast<FIRST_DIGIT>(1)]
                      [static_cast<SECOND_DIGIT>(7)].index);
  
    EXPECT_EQ(7, adder[static_cast<SECOND_DIGIT>(7)]
                      [static_cast<FIRST_DIGIT>(1)].index);
  
    EXPECT_EQ(7, data[static_cast<FIRST_DIGIT>(1)]
                     [static_cast<SECOND_DIGIT>(7)].index);
  
    EXPECT_EQ(7, data[static_cast<SECOND_DIGIT>(7)]
                     [static_cast<FIRST_DIGIT>(1)].index);
  }).Wait();
}
    
  // The return value from `Transaction()` is wrapped into a `Future<>`,
  // use `.Go()` to retrieve the result.
  // (Or `.Wait()` to just wait for the passed in function to complete.)
{
  Future<std::string> future = api.Transaction([](PrimesAPI::T_DATA data) {
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
  // Fill in all the primes and traverse the matrix by rows and by cols.
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto is_prime = [](int i) {
      for (int j = 2; j * j <= i; ++j) {
        if ((i % j) == 0) {
          return false;
        }
      }
      return true;
    };
    int index = 0;
    for (int p = 2; p <= 99; ++p) {
      if (is_prime(p)) {
        data.Add(PrimeCell(p / 10, p % 10, ++index));
      }
    }
  });  // No need to wait, tests that use this data are right here.

  // Primes that start with `4`.
  EXPECT_EQ("41,43,47",
           api.Transaction([](PrimesAPI::T_DATA data) {
             // TODO(dkorolev): In real life, the order can be off. -- D.K.
             std::ostringstream os;
             for (const auto cit : data[static_cast<FIRST_DIGIT>(4)]) {
               os << ',' << (static_cast<int>(cit.row) * 10 +
                             static_cast<int>(cit.col));
             }
             return os.str().substr(1);
           }).Go());
   
  // Primes that end with `7`.
  EXPECT_EQ("7,17,37,47,67,97",
           api.Transaction([](PrimesAPI::T_DATA data) {
             // TODO(dkorolev): In real life, the order can be off. -- D.K.
             std::ostringstream os;
             for (const auto cit : data[static_cast<SECOND_DIGIT>(7)]) {
               os << ',' << (static_cast<int>(cit.row) * 10 +
                             static_cast<int>(cit.col));
             }
             return os.str().substr(1);
           }).Go());
}
   
{
  // Confirm that the send parameter callback is `std::move()`-d
  // into the processing thread.
  // Interestingly, a REST-ful endpoint is the easiest possible test.
  HTTP(port).Register("/key_entry", [&api](Request request) {
    api.GetWithNext(static_cast<PRIME>(FromString<int>(request.url.query["p"])),
                    std::move(request));
  });
  auto response_prime = HTTP(GET(Printf("http://localhost:%d/key_entry?p=7", port)));
  EXPECT_EQ(200, static_cast<int>(response_prime.code));
  EXPECT_EQ("{\"entry\":{\"ms\":42,\"prime\":7,\"index\":4}}\n", response_prime.body);
  auto response_composite = HTTP(GET(Printf("http://localhost:%d/key_entry?p=9", port)));
  EXPECT_EQ(404, static_cast<int>(response_composite.code));
  EXPECT_EQ("{\"error\":\"NOT_FOUND\"}\n", response_composite.body);
  
  HTTP(port).Register("/matrix_entry_1", [&api](Request request) {
    const auto a = static_cast<FIRST_DIGIT>(FromString<int>(request.url.query["a"]));
    const auto b = static_cast<SECOND_DIGIT>(FromString<int>(request.url.query["b"]));
    api.GetWithNext(std::tie(a, b), std::move(request));
  });
  HTTP(port).Register("/matrix_entry_2", [&api](Request request) {
    const auto a = static_cast<FIRST_DIGIT>(FromString<int>(request.url.query["a"]));
    const auto b = static_cast<SECOND_DIGIT>(FromString<int>(request.url.query["b"]));
    api.GetWithNext(a, b, std::move(request));
  });
  auto cell_prime_1 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_1?a=0&b=3", port)));
  EXPECT_EQ(200, static_cast<int>(cell_prime_1.code));
  EXPECT_EQ("{\"entry\":{\"ms\":42,\"d1\":0,\"d2\":3,\"index\":2}}\n", cell_prime_1.body);
  auto cell_prime_2 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_2?a=0&b=3", port)));
  EXPECT_EQ(200, static_cast<int>(cell_prime_2.code));
  EXPECT_EQ("{\"entry\":{\"ms\":42,\"d1\":0,\"d2\":3,\"index\":2}}\n", cell_prime_2.body);
  auto cell_composite_1 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_1?a=0&b=4", port)));
  EXPECT_EQ(404, static_cast<int>(cell_composite_1.code));
  EXPECT_EQ("{\"error\":\"NOT_FOUND\"}\n", cell_composite_1.body);
  auto cell_composite_2 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_2?a=0&b=4", port)));
  EXPECT_EQ(404, static_cast<int>(cell_composite_2.code));
  EXPECT_EQ("{\"error\":\"NOT_FOUND\"}\n", cell_composite_2.body);
}
  
if (0) {
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
}
