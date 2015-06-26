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

#include "../../Blocks/HTTP/api.h"

#include "../../Bricks/strings/join.h"
#include "../../Bricks/strings/printf.h"
#include "../../Bricks/dflags/dflags.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(yoda_docu_test_port, 8999, "");

using yoda::Padawan;
using yoda::MemoryOnlyAPI;
using yoda::Future;
using yoda::Dictionary;
using yoda::Matrix;
using yoda::EntryWrapper;
using yoda::NonexistentEntryAccessed;
using yoda::KeyNotFoundException;
using yoda::KeyAlreadyExistsException;
using yoda::CellNotFoundException;
using yoda::CellAlreadyExistsException;
using yoda::SubscriptException;

using bricks::strings::Join;
using bricks::strings::Printf;
using bricks::strings::FromString;

template<typename T, typename... TS> using CWT = bricks::weed::call_with_type<T, TS...>;

// TODO(dkorolev): Productionize top-level `Has()` and `Value()`.
// TODO(dkorolev): Productionize `Map()`, and also `Filter()` and `Reduce()`.
template<typename T, typename F>
std::vector<CWT<F, decltype(*std::declval<T>().begin())>> Map(const T& container, F&& function) {
typedef CWT<F, decltype(*std::declval<T>().begin())> T_ELEMENT;
std::vector<T_ELEMENT> result;
result.reserve(container.size());
for (const auto& cit : container) { result.push_back(function(cit)); }
return result;
}

  // Explicitly declare { `PRIME`, `Div10` and `Mod10` }
  // types as different. This enables a bit more Yoda magic.
  using PRIME = int;
  
  struct Div10 {
    int div10;
    Div10(int div10 = 0) : div10(div10) {}
    operator int() const { return div10; }
    size_t Hash() const { return div10; }
    template<typename A>
    void serialize(A& ar) {
      ar(CEREAL_NVP(div10));
    }
  };
    
  struct Mod10 {
    int mod10;
    Mod10(int mod10 = 0) : mod10(mod10) {}
    operator int() const { return mod10; }
    size_t Hash() const { return mod10; }
    template<typename A>
    void serialize(A& ar) {
      ar(CEREAL_NVP(mod10));
    }
  };
  
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
      ar(CEREAL_NVP(prime), CEREAL_NVP(index));
    }
  };
  CEREAL_REGISTER_TYPE(Prime);
  
  // Serializable class `PrimeCell`.
  struct PrimeCell : Padawan {
    Div10 row;
    Mod10 col;
    int index;
  
    PrimeCell(const int a = 0, const int b = 0, const int index = 0)
      : row(Div10(a)),
        col(Mod10(b)),
        index(index) {
    }
  
    PrimeCell(const PrimeCell&) = default;
  
    template <typename A>
    void serialize(A& ar) {
      Padawan::serialize(ar);
      ar(CEREAL_NVP(row), CEREAL_NVP(col), CEREAL_NVP(index));
    }
  };
  CEREAL_REGISTER_TYPE(PrimeCell);
  
  // A more complex case of the using a non-POD type for the key.
  struct StringIntTuple : Padawan {
    std::string key;
    int value;
    StringIntTuple(const std::string& key = "", int value = 0) : key(key), value(value) {}
    template <typename A>
    void serialize(A& ar) {
      Padawan::serialize(ar);
      ar(CEREAL_NVP(key), CEREAL_NVP(value));
    }
  };
  CEREAL_REGISTER_TYPE(StringIntTuple);
  
TEST(YodaDocu, Test) {
const int port = FLAGS_yoda_docu_test_port;
bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(42));
HTTP(port).ResetAllHandlers();

  // Define the `api` object.
  typedef MemoryOnlyAPI<Dictionary<StringIntTuple>,
                        Dictionary<Prime>,
                        Matrix<PrimeCell>> PrimesAPI;
  PrimesAPI api("YodaExampleUsage");
  
  // Simple dictionary usecase, and a unit test for type system implementation.
  api.Add(StringIntTuple("two", 2));
  api.Add(static_cast<const StringIntTuple&>(StringIntTuple("three", 3)));
  api.Transaction([](PrimesAPI::T_DATA data) {
    data.Add(StringIntTuple("five", 5));
    data.Add(static_cast<const StringIntTuple&>(
      StringIntTuple("seven", 7)));
    EXPECT_EQ(3, static_cast<StringIntTuple>(
      data.Get(std::string("three"))).value);
    EXPECT_EQ(5, static_cast<StringIntTuple>(
      data.Get(static_cast<const std::string&>(std::string("five")))).value);
  });
  EXPECT_EQ(2, static_cast<StringIntTuple>(
    api.Get(std::string("two")).Go()).value);
  EXPECT_EQ(7, static_cast<StringIntTuple>(
    api.Get(static_cast<const std::string&>(std::string("seven"))).Go()).value);
  
  // `2` is the first prime.
  // `.Go()` (or `.Wait()`) makes `Add()` a blocking call.
  api.Add(Prime(2, 1)).Go();
  
  // `3` is the second prime.
  // `api.Add()` never throws and silently overwrites.
  api.Add(Prime(3, 100));
  api.Add(Prime(3, 2));
  
  // `api.Has()` supports both raw keys and tuples.
  ASSERT_TRUE(api.Has(static_cast<PRIME>(3)).Go());
  ASSERT_TRUE(api.Has(std::make_tuple(static_cast<PRIME>(3))).Go());
  ASSERT_FALSE(api.Has(static_cast<PRIME>(4)).Go());
  
  // `api.Get()` has multiple signatures, one or more per supported data type.
  // It never throws, and returns a wrapper that can be cast to both `bool`
  // and the underlying type.
  ASSERT_TRUE(static_cast<bool>(api.Get(static_cast<PRIME>(2)).Go()));
  ASSERT_TRUE(static_cast<bool>(api.Get(std::make_tuple(static_cast<PRIME>(2))).Go()));
  EXPECT_EQ(1, static_cast<const Prime&>(api.Get(static_cast<PRIME>(2)).Go()).index);
  ASSERT_TRUE(static_cast<bool>(api.Get(static_cast<PRIME>(3)).Go()));
  EXPECT_EQ(2, static_cast<const Prime&>(api.Get(static_cast<PRIME>(3)).Go()).index);
  ASSERT_FALSE(static_cast<bool>(api.Get(static_cast<PRIME>(4)).Go()));
  
  // Expanded syntax for `Add()`.
{
  api.Transaction([](PrimesAPI::T_DATA data) {
    Dictionary<Prime>::Mutator(data).Add(Prime(5, 3));
  }).Wait();
  
  api.Transaction([](PrimesAPI::T_DATA data) {
    Dictionary<Prime>::Mutator(data).Add(Prime(7, 100));
  }).Wait();
  
  // `Add()`: Overwrite is OK.
  api.Transaction([](PrimesAPI::T_DATA data) {
    Dictionary<Prime>::Mutator(data).Add(Prime(7, 4));
  }).Wait();
}
    
  // Expanded syntax for `Get()`.
{
  Future<EntryWrapper<Prime>> future2 = api.Transaction([](PrimesAPI::T_DATA data) {
    return Dictionary<Prime>::Accessor(data).Get(static_cast<PRIME>(2));
  });
  EntryWrapper<Prime> entry2 = future2.Go();
  
  const bool b2 = entry2;
  ASSERT_TRUE(b2);
  
  const Prime& p2 = entry2;
  EXPECT_EQ(1, p2.index);
  
  Future<EntryWrapper<Prime>> future5 = api.Transaction([](PrimesAPI::T_DATA data) {
    return Dictionary<Prime>::Accessor(data).Get(static_cast<PRIME>(5));
  });
  EntryWrapper<Prime> entry5 = future5.Go();
  
  const bool b5 = entry5;
  ASSERT_TRUE(b5);
  
  const Prime& p5 = entry5;
  EXPECT_EQ(3, p5.index);
  
  Future<EntryWrapper<Prime>> future7 = api.Transaction([](PrimesAPI::T_DATA data) {
    return Dictionary<Prime>::Accessor(data).Get(static_cast<PRIME>(7));
  });
  EntryWrapper<Prime> entry7 = future7.Go();
  
  const bool b7 = entry7;
  ASSERT_TRUE(b7);
  
  const Prime& p7 = entry7;
  EXPECT_EQ(4, p7.index);
  
  Future<EntryWrapper<Prime>> future8 = api.Transaction([](PrimesAPI::T_DATA data) {
    return Dictionary<Prime>::Accessor(data).Get(static_cast<PRIME>(8));
  });
  EntryWrapper<Prime> entry8 = future8.Go();
  
  const bool b8 = entry8;
  ASSERT_FALSE(b8);
}
  
  // Accessing the memory view of `data`.
{
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto getter = Dictionary<Prime>::Accessor(data);
    auto adder = Dictionary<Prime>::Mutator(data);
  
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

    // Check if the value exists.
    ASSERT_TRUE(getter.Has(static_cast<PRIME>(13)));

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
    ASSERT_TRUE(data.Has(std::make_tuple(static_cast<PRIME>(3))));
    ASSERT_TRUE(data.Has(static_cast<PRIME>(3)));
    ASSERT_TRUE(static_cast<bool>(data.Get(std::make_tuple(static_cast<PRIME>(3)))));
    ASSERT_TRUE(static_cast<bool>(data.Get(static_cast<PRIME>(3))));
    EXPECT_EQ(2, static_cast<const Prime&>(data.Get(static_cast<PRIME>(3))).index);
    ASSERT_FALSE(data.Has(static_cast<PRIME>(4)));
    ASSERT_FALSE(static_cast<bool>(data.Get(static_cast<PRIME>(4))));
    EXPECT_EQ(3, data[static_cast<PRIME>(5)].index);
    ASSERT_THROW(data[static_cast<PRIME>(9)],
                 KeyNotFoundException<Prime>);
  
    // Traversal.
    EXPECT_EQ(10u, getter.size());
    EXPECT_EQ(10u, adder.size());
  
    std::set<std::pair<int, int>> set;  // To ensure the order is right.
    for (auto cit = getter.begin(); cit != getter.end(); ++cit) {
      set.insert(std::make_pair((*cit).index, static_cast<int>(cit->prime)));
    }
    EXPECT_EQ("1:2,2:3,3:5,4:7,5:11,6:13,7:17,9:19,10:23,11:29",
              Join(Map(set, [](const std::pair<int, int>& e) {
                         return Printf("%d:%d", e.first, e.second);
                       }), ','));
      
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
  // Work with `Matrix<>` as well.
  api.Add(PrimeCell(0, 2, 100));
  api.Add(PrimeCell(0, 2, 1));  // Overwrite.
  api.Add(PrimeCell(0, 3, 2));
  
  const EntryWrapper<PrimeCell> e2 = api.Get(Div10(0), Mod10(2)).Go();
  const bool b2 = e2;
  ASSERT_TRUE(b2);
  ASSERT_TRUE(api.Has(Div10(0), Mod10(2)).Go());
  const PrimeCell p2 = e2;
  EXPECT_EQ(0, static_cast<int>(p2.row));
  EXPECT_EQ(2, static_cast<int>(p2.col));
  EXPECT_EQ(1, p2.index);
  
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto getter = Matrix<PrimeCell>::Accessor(data);
    auto adder = Matrix<PrimeCell>::Mutator(data);
  
    const EntryWrapper<PrimeCell> e3 = getter.Get(Div10(0), Mod10(3));
    const bool b3 = e3;
    ASSERT_TRUE(b3);
    ASSERT_TRUE(getter.Has(Div10(0), Mod10(3)));
    const PrimeCell p3 = e3;
    EXPECT_EQ(0, static_cast<int>(p3.row));
    EXPECT_EQ(3, static_cast<int>(p3.col));
    EXPECT_EQ(2, p3.index);
  
    const EntryWrapper<PrimeCell> e4 = getter.Get(Div10(0), Mod10(4));
    const bool b4 = e4;
    ASSERT_FALSE(b4);
    ASSERT_FALSE(getter.Has(Div10(0), Mod10(4)));
    ASSERT_THROW(static_cast<void>(static_cast<const PrimeCell&>(e4)),
                 NonexistentEntryAccessed);
  
    adder.Add(PrimeCell(0, 5, 3));
    adder.Add(std::tuple<PrimeCell>(PrimeCell(0, 5, 3)));
  
    data.Add(PrimeCell(0, 7, 4));
  
    adder << PrimeCell(1, 1, 5) << PrimeCell(1, 3, 6);
    data << PrimeCell(1, 7, 7);
    ASSERT_THROW(data << PrimeCell(1, 7, 7),
                 CellAlreadyExistsException<PrimeCell>);
  
    const auto key1 = std::make_tuple(Div10(1), Mod10(7));
    EXPECT_EQ(7, getter[key1].index);
  
    const auto key2 = std::make_tuple(Div10(0), Mod10(4));
    ASSERT_THROW(getter[key2],
                 CellNotFoundException<PrimeCell>);
  
    EXPECT_EQ(7, getter[Div10(1)][Mod10(7)].index);
  
    EXPECT_EQ(7, getter[Mod10(7)][Div10(1)].index);
  
    EXPECT_EQ(7, adder[Div10(1)][Mod10(7)].index);
  
    EXPECT_EQ(7, adder[Mod10(7)][Div10(1)].index);
  
    EXPECT_EQ(7, data[Div10(1)][Mod10(7)].index);
  
    EXPECT_EQ(7, data[Mod10(7)][Div10(1)].index);
  }).Wait();
}
    
  // The return value from `Transaction()` is wrapped into a `Future<>`,
  // use `.Go()` to retrieve the result.
  // (Or `.Wait()` to just wait for the passed in function to complete.)
{
  Future<std::string> future = api.Transaction([](PrimesAPI::T_DATA data) {
    const auto getter = Dictionary<Prime>::Accessor(data);
    return Printf("[2]=%d,[3]=%d,[5]*[7]=%d",
                  getter[static_cast<PRIME>(2)].index, 
                  getter[static_cast<PRIME>(3)].index,
                  getter[static_cast<PRIME>(5)].index *
                  getter[static_cast<PRIME>(7)].index);
  });
  EXPECT_EQ("[2]=1,[3]=2,[5]*[7]=12", future.Go());
}
  
  int prime_index = 0;
  const auto is_prime = [](int i) {
    for (int j = 2; j * j <= i; ++j) {
      if ((i % j) == 0) {
        return false;
      }
    }
    return true;
  };
  
{
  // Fill in all the primes and traverse the matrix by rows and by cols.
  api.Transaction([&prime_index, &is_prime](PrimesAPI::T_DATA data) {
    for (int p = 2; p <= 99; ++p) {
      if (is_prime(p)) {
        data.Add(PrimeCell(p / 10, p % 10, ++prime_index));
      }
    }
  });  // No need to wait, tests that use this data are right here.
  
  // Primes that start with `4`.
  EXPECT_EQ("41,43,47",
            api.Transaction([](PrimesAPI::T_DATA data) {
              std::set<int> values;
              for (const auto cit : data[static_cast<Div10>(4)]) {
                values.insert(static_cast<int>(cit.row) * 10 +
                              static_cast<int>(cit.col));
              }
              return Join(values, ',');
            }).Go());
   
  // Primes that end with `7`.
  EXPECT_EQ("7,17,37,47,67,97",
            api.Transaction([](PrimesAPI::T_DATA data) {
              std::set<int> values;
              for (const auto cit : data[Mod10(7)]) {
                values.insert(static_cast<int>(cit.row) * 10 +
                              static_cast<int>(cit.col));
              }
              return Join(values, ',');
            }).Go());
  
  // Top-level per-row and per-column accessors.
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto accessor = Matrix<PrimeCell>::Accessor(data);
    EXPECT_EQ(10u, accessor.Rows().size());
    EXPECT_EQ(6u, accessor.Cols().size());  // 1, 2, 3, 5, 7 or 9, my captain.
    std::set<std::string> by_rows_keys;
    std::set<std::string> by_rows_values;
    for (const auto cit : accessor.Rows()) {
      by_rows_keys.insert(Printf("[`%d`:%d]",
                          static_cast<int>(cit.key()),
                          static_cast<int>(cit.size())));
      std::set<int> indexes;
      for (const auto cit2 : cit) {
        indexes.insert(cit2.index);
      }
      by_rows_values.insert("[" + Join(indexes, ',') + "]");
    }
    EXPECT_EQ(
      "[`0`:4][`1`:4][`2`:2][`3`:2][`4`:3]"
      "[`5`:2][`6`:2][`7`:3][`8`:2][`9`:1]",
      Join(by_rows_keys, ""));
    EXPECT_EQ(
      "[1,2,3,4][11,12][13,14,15][16,17]"
      "[18,19][20,21,22][23,24][25][5,6,7,8][9,10]",
       Join(by_rows_values, ""));
  
    std::set<std::string> by_cols_keys;
    std::set<std::string> by_cols_values;
    for (const auto cit : accessor.Cols()) {
      by_cols_keys.insert(Printf("(`%d`:%d)",
                          static_cast<int>(cit.key()),
                          static_cast<int>(cit.size())));
      std::set<int> values;
      for (const auto cit2 : cit) {
        values.insert(cit2.index);
      }
      by_cols_values.insert("(" + Join(values, ',') + ")");
    }
    EXPECT_EQ("(`1`:5)(`2`:1)(`3`:7)(`5`:1)(`7`:6)(`9`:5)",
              Join(by_cols_keys, ""));
    EXPECT_EQ("(1)(2,6,9,14,16,21,23)(3)(4,7,12,15,19,25)"
              "(5,11,13,18,20)(8,10,17,22,24)",
              Join(by_cols_values, ""));
    
    const auto first_digit_one = Div10(1);
    const auto second_digit_three = Mod10(3);
    const auto first_digit_off = Div10(100);
    const auto second_digit_off = Mod10(100);
    EXPECT_EQ(6, accessor.Rows()[first_digit_one][second_digit_three].index);
    EXPECT_EQ(6, accessor.Cols()[second_digit_three][first_digit_one].index);
    ASSERT_THROW(accessor.Rows()[first_digit_off],
                 SubscriptException<PrimeCell>);
    ASSERT_THROW(accessor.Rows()[first_digit_one][second_digit_off],
                 SubscriptException<PrimeCell>);
    ASSERT_THROW(accessor.Cols()[second_digit_off],
                 SubscriptException<PrimeCell>);
    ASSERT_THROW(accessor.Cols()[second_digit_three][first_digit_off],
                 SubscriptException<PrimeCell>);
  }).Go();
}
  
{
  // Confirm that the underlying storage is an `unordered_map<>`,
  // not an `std::map<>`. To do so, add more prime numbers
  // and verify that their internal order is not lexicographical.
  api.Transaction([&prime_index, &is_prime](PrimesAPI::T_DATA data) {
    for (int p = 100; p <= 999; ++p) {
      if (is_prime(p)) {
        data.Add(PrimeCell(p / 10, p % 10, ++prime_index));
      }
    }
  }).Go();
  EXPECT_EQ(168, prime_index);
  
  api.Transaction([](PrimesAPI::T_DATA data) {
    const auto accessor = Matrix<PrimeCell>::Accessor(data);
    // `Rows()` are now the set of all prime numbers
    // with the last digit chopped off.
    EXPECT_EQ(93u, accessor.Rows().size());
    std::vector<int> vector;
    std::set<int> set;
    for (const auto cit : accessor.Rows()) {
      // It's actually first one or two digits here.
      const Div10 row = cit.key();
      const int row_as_int = static_cast<int>(row);
      vector.push_back(row_as_int);
      set.insert(row_as_int);
    }
    EXPECT_EQ(93u, vector.size());
    EXPECT_EQ(93u, set.size());
    EXPECT_EQ("0,1,2,3,4,5,6,7,8,9,"
              "10,11,12,13,14,15,16,17,18,19,"
              "21,22,23,24,25,26,27,28,29,"     // No 20x.
              "30,31,33,34,35,36,37,38,39,"     // No 32x.
              "40,41,42,43,44,45,46,47,48,49,"
              "50,52,54,55,56,57,58,59,"        // No 51x, 53x.
              "60,61,63,64,65,66,67,68,69,"     // No 62x.
              "70,71,72,73,74,75,76,77,78,79,"
              "80,81,82,83,85,86,87,88,"        // No 84x, 89x.
              "90,91,92,93,94,95,96,97,98,99",
              Join(set, ','));
    EXPECT_NE(Join(vector, ','), Join(set, ','));
  }).Go();
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
  EXPECT_EQ("{\"error\":{\"message\":\"NOT_FOUND\"}}\n", response_composite.body);
  
  HTTP(port).Register("/matrix_entry_1", [&api](Request request) {
    const auto a = Div10(FromString<int>(request.url.query["a"]));
    const auto b = Mod10(FromString<int>(request.url.query["b"]));
    api.GetWithNext(std::tie(a, b), std::move(request));
  });
  HTTP(port).Register("/matrix_entry_2", [&api](Request request) {
    const auto a = Div10(FromString<int>(request.url.query["a"]));
    const auto b = Mod10(FromString<int>(request.url.query["b"]));
    api.GetWithNext(a, b, std::move(request));
  });
  auto cell_prime_1 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_1?a=0&b=3", port)));
  EXPECT_EQ(200, static_cast<int>(cell_prime_1.code));
  EXPECT_EQ("{\"entry\":{\"ms\":42,\"row\":{\"div10\":0},\"col\":{\"mod10\":3},\"index\":2}}\n",
            cell_prime_1.body);
  auto cell_prime_2 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_2?a=0&b=3", port)));
  EXPECT_EQ(200, static_cast<int>(cell_prime_2.code));
  EXPECT_EQ("{\"entry\":{\"ms\":42,\"row\":{\"div10\":0},\"col\":{\"mod10\":3},\"index\":2}}\n",
            cell_prime_2.body);
  auto cell_composite_1 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_1?a=0&b=4", port)));
  EXPECT_EQ(404, static_cast<int>(cell_composite_1.code));
  EXPECT_EQ("{\"error\":{\"message\":\"NOT_FOUND\"}}\n", cell_composite_1.body);
  auto cell_composite_2 = HTTP(GET(Printf("http://localhost:%d/matrix_entry_2?a=0&b=4", port)));
  EXPECT_EQ(404, static_cast<int>(cell_composite_2.code));
  EXPECT_EQ("{\"error\":{\"message\":\"NOT_FOUND\"}}\n", cell_composite_2.body);
}
  
{
  // Confirm that the stream is indeed populated.
  api.ExposeViaHTTP(port, "/data");
  EXPECT_EQ(
#if 1
"{\"data\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"StringIntTuple\",\"ptr_wrapper\":{\"valid\":1,\"data\":{\"ms\":42,\"key\":\"two\",\"value\":2}}}}\n",
#else
    "... JSON represenation of the first entry ...",
#endif
    HTTP(GET(Printf("http://localhost:%d/data?cap=1", port))).body);
  EXPECT_EQ(
#if 1
"{\"data\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"PrimeCell\",\"ptr_wrapper\":{\"valid\":1,\"data\":{\"ms\":42,\"row\":{\"div10\":99},\"col\":{\"mod10\":7},\"index\":168}}}}\n",
#else
    "... JSON represenation of the last entry ...",
#endif
    HTTP(GET(Printf("http://localhost:%d/data?n=1", port))).body);
}
}
