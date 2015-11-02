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

#include "PSYKHANOOL.h"

#include "../Bricks/file/file.h"
#include "../Bricks/dflags/dflags.h"
#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(PSYKHANOOL_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

#define USE_KEY_METHODS

namespace PSYKHANOOL_test {

struct Element final {
  int x;
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(x));
  }
};

struct Record final {
#ifdef USE_KEY_METHODS
  std::string lhs;
  const std::string& key() const { return lhs; }
  void set_key(const std::string& key) { lhs = key; }
#else
  std::string key;
#endif
  int rhs;
  template <typename A>
  void serialize(A& ar) {
    ar(
#ifdef USE_KEY_METHODS
        CEREAL_NVP(lhs),
#else
        CEREAL_NVP(key),
#endif
        CEREAL_NVP(rhs));
  }
};

struct Cell final {
#ifdef USE_KEY_METHODS
  int foo;
  int row() const { return foo; }
  void set_row(int row) { foo = row; }
#else
  int row;
#endif

#ifdef USE_KEY_METHODS
  std::string bar;
  const std::string& col() const { return bar; }
  void set_col(const std::string& col) { bar = col; }
#else
  std::string col;
#endif

  int phew;

  template <typename A>
  void serialize(A& ar) {
    ar(
#ifdef USE_KEY_METHODS
        CEREAL_NVP(foo),
        CEREAL_NVP(bar),
#else
        CEREAL_NVP(row),
        CEREAL_NVP(col),
#endif
        CEREAL_NVP(phew));
  }
};

}  // namespace PSYKHANOOL_test

// TODO(dkorolev): Make the following work.
//
//   DATABASE(UnitTestStorage) {
//     TABLE(v ,Vector<PSYKHANOOL_test::Element>);
//     TABLE(d, OrderedDictionary<PSYKHANOOL_test::Record>);
//     TABLE(m, LightweightLightweightMatrix<PSYKHANOOL_test::Cell>);
//   };
//   // F*ck yeah!
//
// That simple.

template <typename POLICY>
struct UnitTestStorage final {
  typename POLICY::Instance instance;

  // The initialization with `{instance}` works on both clang++ and g++,
  // and respects initialization order. -- D.K.
  Vector<PSYKHANOOL_test::Element, POLICY> v{"v", instance};
  OrderedDictionary<PSYKHANOOL_test::Record, POLICY> d{"d", instance};
  LightweightMatrix<PSYKHANOOL_test::Cell, POLICY> m{"m", instance};

  template <typename... ARGS>
  UnitTestStorage(ARGS&&... args)
      : instance(std::forward<ARGS>(args)...) {
    instance.Run();
  }
};

// Unit-test that the storage can be used as a storage,
// plus leave some entries handing upon exit to unit-test the persistence layer.
template <typename POLICY>
void RunUnitTest(UnitTestStorage<POLICY>& storage, bool leave_data_behind = false) {
  using namespace PSYKHANOOL_test;

  // Test the logic of `Vector`.
  EXPECT_TRUE(storage.v.Empty());
  EXPECT_EQ(0u, storage.v.Size());
  EXPECT_TRUE(!storage.v[0].Exists());
  EXPECT_TRUE(!Exists(storage.v[0]));
  ASSERT_THROW(Value(storage.v[0]), NoValueException);
  ASSERT_THROW(storage.v[0].Value(), NoValueException);

  storage.v.PushBack(Element{42});

  EXPECT_TRUE(!storage.v.Empty());
  EXPECT_EQ(1u, storage.v.Size());
  EXPECT_TRUE(Exists(storage.v[0]));
  EXPECT_EQ(42, Value(storage.v[0]).x);
  ASSERT_THROW(Value(storage.v[1]), NoValueException);
  ASSERT_THROW(storage.v[1].Value(), NoValueException);

  storage.v.PushBack(Element{1});
  storage.v.PushBack(Element{2});
  storage.v.PushBack(Element{3});

  EXPECT_TRUE(!storage.v.Empty());
  EXPECT_EQ(4u, storage.v.Size());
  EXPECT_TRUE(Exists(storage.v[3]));
  EXPECT_EQ(3, Value(storage.v[3]).x);

  storage.v.PopBack();

  EXPECT_TRUE(!storage.v.Empty());
  EXPECT_EQ(3u, storage.v.Size());
  EXPECT_TRUE(Exists(storage.v[2]));
  EXPECT_EQ(2, Value(storage.v[2]).x);

  storage.v.PopBack();
  storage.v.PopBack();
  storage.v.PopBack();

  EXPECT_TRUE(storage.v.Empty());
  EXPECT_EQ(0u, storage.v.Size());

  ASSERT_THROW(storage.v.PopBack(), CannotPopBackFromEmptyVectorException);

  // Test the logic of `OrderedDictionary`.
  EXPECT_TRUE(storage.d.Empty());
  EXPECT_EQ(0u, storage.d.Size());
  EXPECT_TRUE(!storage.d["one"].Exists());
  EXPECT_TRUE(!Exists(storage.d["one"]));
  ASSERT_THROW(Value(storage.d["one"]), NoValueException);
  ASSERT_THROW(storage.d["one"].Value(), NoValueException);

  {
    size_t count = 0u;
    int value = 0;
    for (const auto& e : storage.d) {
      ++count;
      value += e.rhs;
    }
    EXPECT_EQ(0u, count);
    EXPECT_EQ(0, value);
  }

  storage.d.Insert(Record{"one", 1});

  {
    size_t count = 0u;
    int value = 0;
    for (const auto& e : storage.d) {
      ++count;
      value += e.rhs;
    }
    EXPECT_EQ(1u, count);
    EXPECT_EQ(1, value);
  }

  EXPECT_FALSE(storage.d.Empty());
  EXPECT_EQ(1u, storage.d.Size());
  EXPECT_TRUE(storage.d["one"].Exists());
  EXPECT_TRUE(Exists(storage.d["one"]));
  EXPECT_EQ(1, Value(storage.d["one"]).rhs);
  EXPECT_EQ(1, storage.d["one"].Value().rhs);

  storage.d.Insert(Record{"two", 2});

  EXPECT_FALSE(storage.d.Empty());
  EXPECT_EQ(2u, storage.d.Size());
  EXPECT_EQ(1, Value(storage.d["one"]).rhs);
  EXPECT_EQ(2, Value(storage.d["two"]).rhs);

  {
    size_t count = 0u;
    int value = 0;
    for (const auto& e : storage.d) {
      ++count;
      value += e.rhs;
    }
    EXPECT_EQ(2u, count);
    EXPECT_EQ(1 + 2, value);
  }

  storage.d.Erase("one");

  EXPECT_FALSE(storage.d.Empty());
  EXPECT_EQ(1u, storage.d.Size());
  EXPECT_FALSE(Exists(storage.d["one"]));
  EXPECT_TRUE(Exists(storage.d["two"]));

  storage.d.Erase("two");

  // Test the logic of `LightweightMatrix`.
  EXPECT_TRUE(storage.m.Empty());
  EXPECT_EQ(0u, storage.m.Size());
  EXPECT_TRUE(storage.m.Rows().Empty());
  EXPECT_TRUE(storage.m.Cols().Empty());
  EXPECT_FALSE(storage.m.Rows().Has(1));
  EXPECT_FALSE(storage.m.Cols().Has("one"));
  EXPECT_FALSE(storage.m.Has(1, "one"));

  storage.m.Add(Cell{1, "one", 1});
  storage.m.Add(Cell{2, "two", 2});
  storage.m.Add(Cell{2, "too", 3});
  EXPECT_FALSE(storage.m.Empty());
  EXPECT_EQ(3u, storage.m.Size());
  EXPECT_FALSE(storage.m.Rows().Empty());
  EXPECT_FALSE(storage.m.Cols().Empty());
  EXPECT_TRUE(storage.m.Rows().Has(1));
  EXPECT_TRUE(storage.m.Cols().Has("one"));
  EXPECT_TRUE(storage.m.Has(1, "one"));
  EXPECT_TRUE(Exists(storage.m.Get(1, "one")));
  EXPECT_EQ(1, Value(storage.m.Get(1, "one")).phew);

  {
    std::vector<int> rows;
    for (const auto& e : storage.m.Rows()) {
      rows.push_back(e.Key());
    }
    EXPECT_EQ("1,2", bricks::strings::Join(rows, ','));
  }
  {
    std::vector<std::string> cols;
    for (const auto& e : storage.m.Cols()) {
      cols.push_back(e.Key());
    }
    EXPECT_EQ("one,too,two", bricks::strings::Join(cols, ','));
  }

  storage.m.Delete(1, "one");
  EXPECT_EQ(2u, storage.m.Size());
  EXPECT_FALSE(storage.m.Rows().Has(1));
  EXPECT_FALSE(storage.m.Cols().Has("one"));

  storage.m.Delete(2, "two");
  EXPECT_EQ(1u, storage.m.Size());
  EXPECT_TRUE(storage.m.Rows().Has(2));
  EXPECT_FALSE(storage.m.Cols().Has("two"));
  EXPECT_TRUE(storage.m.Cols().Has("too"));

  storage.m.Delete(2, "too");
  EXPECT_FALSE(storage.m.Rows().Has(2));
  EXPECT_FALSE(storage.m.Cols().Has("too"));

  EXPECT_TRUE(storage.m.Empty());
  EXPECT_EQ(0u, storage.m.Size());
  EXPECT_TRUE(storage.m.Rows().Empty());
  EXPECT_TRUE(storage.m.Cols().Empty());
  EXPECT_FALSE(storage.m.Rows().Has(1));
  EXPECT_FALSE(storage.m.Cols().Has("one"));

  // Test persistence. Leave some entries so that a restarted/replayed storage has some data to it.
  EXPECT_TRUE(storage.v.Empty());
  EXPECT_TRUE(storage.d.Empty());
  EXPECT_TRUE(storage.m.Empty());

  if (leave_data_behind) {
    storage.v.PushBack(Element{100});
    storage.d.Insert(Record{"answer", 42});
    storage.m.Add(Cell{101, "one-oh-one", 1001});
  }
}

TEST(PSYKHANOOL, InMemory) {
  UnitTestStorage<InMemory> in_memory;
  RunUnitTest(in_memory);
}

TEST(PSYKHANOOL, OnDisk) {
  const std::string persistence_file_name = bricks::FileSystem::JoinPath(FLAGS_PSYKHANOOL_test_tmpdir, "data");
  const auto persistence_file_remover = bricks::FileSystem::ScopedRmFile(persistence_file_name);

  {
    // First, run the unit test in a way that preserves the database intact.
    UnitTestStorage<ReplayFromAndAppendToFile> virgin(persistence_file_name);
    RunUnitTest(virgin);
  }

  {
    // Running the above should pass any number of times, because purity.
    UnitTestStorage<ReplayFromAndAppendToFile> petting(persistence_file_name);
    RunUnitTest(petting);
  }

  {
    // Now, run the unit test, that leaves some data behind ...
    UnitTestStorage<ReplayFromAndAppendToFile> defloration(persistence_file_name);
    RunUnitTest(defloration, true);
  }

  {
    // ... and confirm that data can be seen when resuming from the snapshot.
    UnitTestStorage<ReplayFromAndAppendToFile> resumed(persistence_file_name);

    EXPECT_EQ(1u, resumed.v.Size());
    EXPECT_TRUE(Exists(resumed.v[0]));
    EXPECT_EQ(100, Value(resumed.v[0]).x);

    EXPECT_EQ(1u, resumed.d.Size());
    EXPECT_TRUE(resumed.d["answer"].Exists());
    EXPECT_TRUE(Exists(resumed.d["answer"]));
    EXPECT_EQ(42, Value(resumed.d["answer"]).rhs);

    EXPECT_EQ(1u, resumed.m.Size());
    EXPECT_TRUE(resumed.m.Rows().Has(101));
    EXPECT_TRUE(Exists(resumed.m.Rows()[101]));
    EXPECT_FALSE(Value(resumed.m.Rows()[101]).Empty());
    EXPECT_EQ(1u, Value(resumed.m.Rows()[101]).Size());
    EXPECT_EQ(1001, Value(resumed.m.Rows()[101]).begin()->phew);
    EXPECT_TRUE(resumed.m.Cols().Has("one-oh-one"));
    EXPECT_FALSE(Value(resumed.m.Cols()["one-oh-one"]).Empty());
    EXPECT_EQ(1u, Value(resumed.m.Cols()["one-oh-one"]).Size());
    EXPECT_EQ(1001, Value(resumed.m.Cols()["one-oh-one"]).begin()->phew);
    EXPECT_TRUE(resumed.m.Has(101, "one-oh-one"));
    EXPECT_TRUE(Exists(resumed.m.Get(101, "one-oh-one")));
    EXPECT_EQ(1001, Value(resumed.m.Get(101, "one-oh-one")).phew);
  }
}
