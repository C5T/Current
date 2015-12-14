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

#include "storage.h"

#include "../Bricks/file/file.h"
#include "../Bricks/dflags/dflags.h"
#include "../3rdparty/gtest/gtest-main-with-dflags.h"

#ifndef _MSC_VER
DEFINE_string(transactional_storage_test_tmpdir,
              ".current",
              "Local path for the test to create temporary files in.");
#else
DEFINE_string(transactional_storage_test_tmpdir,
              "Debug",
              "Local path for the test to create temporary files in.");
#endif

#define USE_KEY_METHODS

namespace transactional_storage_test {

CURRENT_STRUCT(Element) {
  CURRENT_FIELD(x, int32_t);
  CURRENT_CONSTRUCTOR(Element)(int32_t x = 0) : x(x) {}
};

CURRENT_STRUCT(Record) {
#ifdef USE_KEY_METHODS
  CURRENT_FIELD(lhs, std::string);
  const std::string& key() const { return lhs; }
  void set_key(const std::string& key) { lhs = key; }
#else
  CURRENT_FIELD(key, std::string);
#endif
  CURRENT_FIELD(rhs, int32_t);

#ifdef USE_KEY_METHODS
  CURRENT_CONSTRUCTOR(Record)(const std::string& lhs = "", int32_t rhs = 0) : lhs(lhs), rhs(rhs) {}
#else
  CURRENT_CONSTRUCTOR(Record)(const std::string& key = "", int32_t rhs = 0) : key(key), rhs(rhs) {}
#endif
};

CURRENT_STRUCT(Cell) {
#ifdef USE_KEY_METHODS
  CURRENT_FIELD(foo, int32_t);
  int32_t row() const { return foo; }
  void set_row(int32_t row) { foo = row; }
#else
  CURRENT_FIELD(row, int32_t);
#endif

#ifdef USE_KEY_METHODS
  CURRENT_FIELD(bar, std::string);
  const std::string& col() const { return bar; }
  void set_col(const std::string& col) { bar = col; }
#else
  CURRENT_FIELD(col, std::string);
#endif

  CURRENT_FIELD(phew, int32_t);

#ifdef USE_KEY_METHODS
  CURRENT_CONSTRUCTOR(Cell)(int32_t foo = 0, const std::string& bar = "", int32_t phew = 0)
      : foo(foo), bar(bar), phew(phew) {}
#else
  CURRENT_CONSTRUCTOR(Cell)(int32_t row = 0, const std::string& bar = "", int32_t phew = 0)
      : row(row), bar(bar), phew(phew) {}
#endif
};

CURRENT_STORAGE_STRUCT_ALIAS(Element, ElementVector1);
CURRENT_STORAGE_STRUCT_ALIAS(Element, ElementVector2);
CURRENT_STORAGE_STRUCT_ALIAS(Record, RecordDictionary);

using current::storage::container::Ordered;
CURRENT_STORAGE(NewStorageDefinition) {
  CURRENT_STORAGE_FIELD(v1, Vector, ElementVector1);
  CURRENT_STORAGE_FIELD(v2, Vector, ElementVector2);
  CURRENT_STORAGE_FIELD(d, Dictionary, RecordDictionary, Ordered);
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, NewStorageDefinition) {
  using namespace transactional_storage_test;
  using NewStorage = NewStorageDefinition<JSONFilePersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    NewStorage storage(persistence_file_name);
    EXPECT_EQ(3u, storage.FieldsCount());
    storage.Transaction([](CurrentStorage<NewStorage> fields) {
      EXPECT_TRUE(fields.v1.Empty());
      EXPECT_TRUE(fields.v2.Empty());
      fields.v1.PushBack(Element(0));
      fields.v1.PopBack();
      fields.v1.PushBack(Element(42));
      fields.v2.PushBack(Element(100));
      EXPECT_EQ(1u, fields.v1.Size());
      EXPECT_EQ(1u, fields.v2.Size());
      EXPECT_EQ(42, Value(fields.v1[0]).x);
      EXPECT_EQ(100, Value(fields.v2[0]).x);
    });

    storage.Transaction([](CurrentStorage<NewStorage> fields) {
      fields.d.Add(Record{"one", 1});

      {
        size_t count = 0u;
        int32_t value = 0;
        for (const auto& e : fields.d) {
          ++count;
          value += e.rhs;
        }
        EXPECT_EQ(1u, count);
        EXPECT_EQ(1, value);
      }

      EXPECT_FALSE(fields.d.Empty());
      EXPECT_EQ(1u, fields.d.Size());
      EXPECT_TRUE(Exists(fields.d["one"]));
      EXPECT_EQ(1, Value(fields.d["one"]).rhs);

      fields.d.Add(Record{"two", 2});

      EXPECT_FALSE(fields.d.Empty());
      EXPECT_EQ(2u, fields.d.Size());
      EXPECT_EQ(1, Value(fields.d["one"]).rhs);
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);

      fields.d.Add(Record{"three", 3});
      fields.d.Erase("three");
    });

    storage.Transaction([](CurrentStorage<NewStorage> fields) {
      fields.v1.PushBack(Element(1));
      fields.v2.PushBack(Element(2));
      fields.d.Add(Record{"three", 3});
      throw std::logic_error("rollback, please");
    });
  }

  {
    NewStorage replayed(persistence_file_name);
    replayed.Transaction([](CurrentStorage<NewStorage> fields) {
      EXPECT_EQ(1u, fields.v1.Size());
      EXPECT_EQ(1u, fields.v2.Size());
      EXPECT_EQ(42, Value(fields.v1[0]).x);
      EXPECT_EQ(100, Value(fields.v2[0]).x);

      EXPECT_FALSE(fields.d.Empty());
      EXPECT_EQ(2u, fields.d.Size());
      EXPECT_EQ(1, Value(fields.d["one"]).rhs);
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);
      EXPECT_FALSE(Exists(fields.d["three"]));
    });
  }
}

#if 0

template <typename POLICY>
struct UnitTestStorage final {
  typename POLICY::Instance instance;

  // The initialization with `{instance}` works on both clang++ and g++,
  // and respects initialization order. -- D.K.
  Vector<transactional_storage_test::Element, POLICY> v{"v", instance};
  OrderedDictionary<transactional_storage_test::Record, POLICY> d{"d", instance};
  LightweightMatrix<transactional_storage_test::Cell, POLICY> m{"m", instance};

  template <typename... ARGS>
  UnitTestStorage(ARGS&&... args)
      : instance(std::forward<ARGS>(args)...) {
    instance.Run();
  }
};

// Unit-test that the storage can be used as a storage,
// plus leave some entries hanging upon exit to unit-test the persistence layer.
template <typename POLICY>
void RunUnitTest(UnitTestStorage<POLICY>& storage, bool leave_data_behind = false) {
  using namespace transactional_storage_test;

  // Test the logic of `Vector`.
  EXPECT_TRUE(storage.v.Empty());
  EXPECT_EQ(0u, storage.v.Size());
  EXPECT_TRUE(!Exists(storage.v[0]));
  ASSERT_THROW(Value(storage.v[0]), NoValueException);

  storage.v.PushBack(Element{42});

  EXPECT_TRUE(!storage.v.Empty());
  EXPECT_EQ(1u, storage.v.Size());
  EXPECT_TRUE(Exists(storage.v[0]));
  EXPECT_EQ(42, Value(storage.v[0]).x);
  ASSERT_THROW(Value(storage.v[1]), NoValueException);

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
  EXPECT_TRUE(!Exists(storage.d["one"]));
  ASSERT_THROW(Value(storage.d["one"]), NoValueException);

  {
    size_t count = 0u;
    int32_t value = 0;
    for (const auto& e : storage.d) {
      // LCOV_EXCL_START
      ++count;
      value += e.rhs;
      // LCOV_EXCL_STOP
    }
    EXPECT_EQ(0u, count);
    EXPECT_EQ(0, value);
  }

  storage.d.Insert(Record{"one", 1});

  {
    size_t count = 0u;
    int32_t value = 0;
    for (const auto& e : storage.d) {
      ++count;
      value += e.rhs;
    }
    EXPECT_EQ(1u, count);
    EXPECT_EQ(1, value);
  }

  EXPECT_FALSE(storage.d.Empty());
  EXPECT_EQ(1u, storage.d.Size());
  EXPECT_TRUE(Exists(storage.d["one"]));
  EXPECT_EQ(1, Value(storage.d["one"]).rhs);

  storage.d.Insert(Record{"two", 2});

  EXPECT_FALSE(storage.d.Empty());
  EXPECT_EQ(2u, storage.d.Size());
  EXPECT_EQ(1, Value(storage.d["one"]).rhs);
  EXPECT_EQ(2, Value(storage.d["two"]).rhs);

  {
    size_t count = 0u;
    int32_t value = 0;
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
    std::vector<int32_t> rows;
    for (const auto& e : storage.m.Rows()) {
      rows.push_back(e.Key());
    }
    EXPECT_EQ("1,2", current::strings::Join(rows, ','));
  }
  {
    std::vector<std::string> cols;
    for (const auto& e : storage.m.Cols()) {
      cols.push_back(e.Key());
    }
    EXPECT_EQ("one,too,two", current::strings::Join(cols, ','));
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

  // Test persistence. Leave some entries so that a restarted/replayed storage has some data to play with.
  EXPECT_TRUE(storage.v.Empty());
  EXPECT_TRUE(storage.d.Empty());
  EXPECT_TRUE(storage.m.Empty());

  if (leave_data_behind) {
    storage.v.PushBack(Element{100});
    storage.d.Insert(Record{"answer", 42});
    storage.m.Add(Cell{101, "one-oh-one", 1001});
  }
}

TEST(TransactionalStorage, InMemory) {
  UnitTestStorage<InMemory> in_memory;
  RunUnitTest(in_memory);
}

TEST(TransactionalStorage, OnDisk) {
  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

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

#endif
