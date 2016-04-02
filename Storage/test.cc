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

#include <set>

#include "docu/docu_2_code.cc"
#include "docu/docu_3_code.cc"

#include "storage.h"
#include "api.h"

#include "rest/hypermedia.h"

#include "../Blocks/HTTP/api.h"

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

DEFINE_int32(transactional_storage_test_port,
             PickPortForUnitTest(),
             "Local port to run [REST] API tests against.");

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

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, Record, RecordDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedMatrix, Cell, CellMatrix);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOne2One, Cell, CellOne2One);

CURRENT_STORAGE(TestStorage) {
  CURRENT_STORAGE_FIELD(d, RecordDictionary);
  CURRENT_STORAGE_FIELD(m, CellMatrix);
  CURRENT_STORAGE_FIELD(o, CellOne2One);
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, SmokeTest) {
  using namespace transactional_storage_test;
  using Storage = TestStorage<JSONFilePersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    EXPECT_EQ(3u, Storage::FIELDS_COUNT);
    Storage storage(persistence_file_name);

    {
      const auto result = storage.Transaction([](MutableFields<Storage> fields) {
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
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }
    {
      const auto result = storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.m.Empty());
        EXPECT_EQ(0u, fields.m.Size());
        EXPECT_TRUE(fields.m.Rows().Empty());
        EXPECT_TRUE(fields.m.Cols().Empty());
        fields.m.Add(Cell{1, "one", 1});
        fields.m.Add(Cell{2, "two", 2});
        fields.m.Add(Cell{2, "too", 3});
        EXPECT_FALSE(fields.m.Empty());
        EXPECT_EQ(3u, fields.m.Size());
        EXPECT_FALSE(fields.m.Rows().Empty());
        EXPECT_FALSE(fields.m.Cols().Empty());
        EXPECT_TRUE(fields.m.Rows().Has(1));
        EXPECT_TRUE(fields.m.Cols().Has("one"));
        EXPECT_TRUE(Exists(fields.m.Get(1, "one")));
        EXPECT_EQ(1, Value(fields.m.Get(1, "one")).phew);
        EXPECT_EQ(2, Value(fields.m.Get(2, "two")).phew);
        EXPECT_EQ(3, Value(fields.m.Get(2, "too")).phew);
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }
    {
      const auto result = storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.o.Empty());
        EXPECT_EQ(0u, fields.o.Size());
        EXPECT_TRUE(fields.o.Rows().Empty());
        EXPECT_TRUE(fields.o.Cols().Empty());
        fields.o.Add(Cell{1, "one", 1});
        fields.o.Add(Cell{2, "two", 2});
        fields.o.Add(Cell{2, "too", 3});
        fields.o.Add(Cell{3, "too", 4});
        EXPECT_FALSE(fields.o.Empty());
        EXPECT_EQ(2u, fields.o.Size());
        EXPECT_FALSE(fields.o.Rows().Empty());
        EXPECT_FALSE(fields.o.Cols().Empty());
        EXPECT_TRUE(fields.o.Rows().Has(1));
        EXPECT_TRUE(Exists(fields.o.GetRow(1)));
        EXPECT_TRUE(fields.o.Cols().Has("one"));
        EXPECT_TRUE(Exists(fields.o.GetCol("one")));
        EXPECT_FALSE(fields.o.Rows().Has(2));
        EXPECT_FALSE(Exists(fields.o.GetRow(2)));
        EXPECT_FALSE(fields.o.Cols().Has("two"));
        EXPECT_FALSE(Exists(fields.o.GetCol("two")));
        EXPECT_TRUE(Exists(fields.o.Get(3, "too")));
        EXPECT_FALSE(Exists(fields.o.Get(2, "too")));
        EXPECT_EQ(1, Value(fields.o.Get(1, "one")).phew);
        EXPECT_EQ(1, Value(fields.o.GetRow(1)).phew);
        EXPECT_EQ(4, Value(fields.o.Get(3, "too")).phew);
        EXPECT_EQ(4, Value(fields.o.GetCol("too")).phew);
        EXPECT_TRUE(fields.o.CanAdd(2, "two"));
        EXPECT_FALSE(fields.o.CanAdd(1, "three"));
        EXPECT_FALSE(fields.o.CanAdd(4, "one"));
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }

    // Iterate over the matrix.
    {
      storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.m.Empty());
        std::multiset<std::string> data;
        for (const auto& row : fields.m.Rows()) {
          for (const auto& element : row) {
            data.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                        current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                        current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data, ' '));
      }).Go();
      storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.m.Empty());
        std::multiset<std::string> data;
        for (const auto& col : fields.m.Cols()) {
          for (const auto& element : col) {
            data.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                        current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                        current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data, ' '));
      }).Go();
    }

    // Iterate over the One2One
    {
      storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.o.Empty());
        std::multiset<std::string> data;
        for (const auto& element : fields.o.Rows()) {
          data.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                      current::ToString(element.phew));
        }
        EXPECT_EQ("1,one=1 3,too=4", current::strings::Join(data, ' '));
      }).Go();
      storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.o.Empty());
        std::multiset<std::string> data;
        for (const auto& element : fields.o.Cols()) {
          data.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                      current::ToString(element.phew));
        }
        EXPECT_EQ("1,one=1 3,too=4", current::strings::Join(data, ' '));
      }).Go();
    }

    // Rollback a transaction involving a `Matrix` and a `One2One`.
    {
      const auto result = storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_EQ(3u, fields.m.Size());
        EXPECT_EQ(2u, fields.m.Rows().Size());
        EXPECT_EQ(3u, fields.m.Cols().Size());

        fields.d.Add(Record{"one", 100});
        fields.d.Add(Record{"four", 4});
        fields.d.Erase("two");

        fields.m.Add(Cell{1, "one", 42});
        fields.m.Add(Cell{3, "three", 3});
        fields.m.Erase(2, "two");

        fields.o.Add(Cell{3, "three", 3});
        fields.o.Add(Cell{4, "four", 4});
        fields.o.EraseRow(1);
        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go();
      EXPECT_FALSE(WasCommitted(result));

      storage.Transaction([](ImmutableFields<Storage> fields) {
        EXPECT_EQ(3u, fields.m.Size());
        EXPECT_EQ(2u, fields.m.Rows().Size());
        EXPECT_EQ(3u, fields.m.Cols().Size());

        EXPECT_EQ(2u, fields.o.Size());
        EXPECT_EQ(2u, fields.o.Rows().Size());
        EXPECT_EQ(2u, fields.o.Cols().Size());
        EXPECT_FALSE(fields.o.Cols().Has("three"));
        EXPECT_TRUE(fields.o.Rows().Has(3));
        EXPECT_TRUE(fields.o.Cols().Has("one"));
      }).Go();
    }

    // Iterate over the matrix with deleted elements, confirm the integrity of `forward_` and `transposed_`.
    {
      EXPECT_FALSE(WasCommitted(storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.m.Rows().Has(2));
        EXPECT_TRUE(fields.m.Cols().Has("two"));
        EXPECT_TRUE(fields.m.Cols().Has("too"));

        fields.m.Erase(2, "two");
        EXPECT_TRUE(fields.m.Rows().Has(2));  // Still has another "2" left, the {2, "too"} key.
        EXPECT_FALSE(fields.m.Cols().Has("two"));
        EXPECT_TRUE(fields.m.Cols().Has("too"));
        EXPECT_EQ(2u, fields.m.Rows().Size());
        EXPECT_EQ(2u, fields.m.Cols().Size());

        fields.m.Erase(2, "too");
        EXPECT_FALSE(fields.m.Rows().Has(2));
        EXPECT_FALSE(fields.m.Cols().Has("two"));
        EXPECT_FALSE(fields.m.Cols().Has("too"));
        EXPECT_EQ(1u, fields.m.Rows().Size());
        EXPECT_EQ(1u, fields.m.Cols().Size());

        std::multiset<std::string> data1;
        std::multiset<std::string> data2;
        for (const auto& row : fields.m.Rows()) {
          for (const auto& element : row) {
            data1.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                         current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                         current::ToString(element.phew));
          }
        }
        for (const auto& col : fields.m.Cols()) {
          for (const auto& element : col) {
            data2.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                         current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                         current::ToString(element.phew));
          }
        }

        EXPECT_EQ("1,one=1", current::strings::Join(data1, ' '));
        EXPECT_EQ("1,one=1", current::strings::Join(data2, ' '));

        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go()));
    }

    // Iterate over the One2One with deleted elements, confirm the integrity of `forward_` and `transposed_`.
    {
      EXPECT_FALSE(WasCommitted(storage.Transaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.o.Rows().Has(1));
        EXPECT_TRUE(fields.o.Cols().Has("one"));
        EXPECT_TRUE(fields.o.Cols().Has("too"));

        fields.o.Add(Cell{3, "two", 5});
        EXPECT_TRUE(fields.o.Rows().Has(3));
        EXPECT_FALSE(fields.o.Cols().Has("too"));
        EXPECT_TRUE(fields.o.Cols().Has("two"));
        EXPECT_EQ(2u, fields.o.Rows().Size());
        EXPECT_EQ(2u, fields.o.Cols().Size());

        fields.o.EraseCol("two");
        EXPECT_FALSE(fields.o.Rows().Has(3));
        EXPECT_FALSE(fields.o.Cols().Has("too"));
        EXPECT_FALSE(fields.o.Cols().Has("two"));
        EXPECT_EQ(1u, fields.o.Rows().Size());
        EXPECT_EQ(1u, fields.o.Cols().Size());

        std::multiset<std::string> rows;
        std::multiset<std::string> cols;
        for (const auto& element : fields.o.Rows()) {
          rows.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                      current::ToString(element.phew));
        }
        for (const auto& element : fields.o.Cols()) {
          cols.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                      current::ToString(element.phew));
        }

        EXPECT_EQ("1,one=1", current::strings::Join(rows, ' '));
        EXPECT_EQ("1,one=1", current::strings::Join(cols, ' '));

        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go()));
    }

    {
      const auto f = [](ImmutableFields<Storage>) { return 42; };

      const auto result = storage.Transaction(f).Go();
      EXPECT_TRUE(WasCommitted(result));
      EXPECT_TRUE(Exists(result));
      EXPECT_EQ(42, Value(result));
    }
  }

  {
    Storage replayed(persistence_file_name);
    replayed.Transaction([](ImmutableFields<Storage> fields) {
      EXPECT_FALSE(fields.d.Empty());
      EXPECT_EQ(2u, fields.d.Size());
      EXPECT_EQ(1, Value(fields.d["one"]).rhs);
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);
      EXPECT_FALSE(Exists(fields.d["three"]));

      EXPECT_FALSE(fields.m.Empty());
      EXPECT_EQ(3u, fields.m.Size());
      EXPECT_EQ(1, Value(fields.m.Get(1, "one")).phew);
      EXPECT_EQ(2, Value(fields.m.Get(2, "two")).phew);
      EXPECT_EQ(3, Value(fields.m.Get(2, "too")).phew);
      EXPECT_FALSE(Exists(fields.m.Get(3, "three")));

      EXPECT_FALSE(fields.o.Empty());
      EXPECT_EQ(2u, fields.o.Size());
      EXPECT_EQ(1, Value(fields.o.Get(1, "one")).phew);
      EXPECT_EQ(1, Value(fields.o.GetCol("one")).phew);
      EXPECT_EQ(4, Value(fields.o.Get(3, "too")).phew);
      EXPECT_EQ(4, Value(fields.o.GetRow(3)).phew);
      EXPECT_FALSE(Exists(fields.o.Get(2, "two")));
      EXPECT_FALSE(Exists(fields.o.GetRow(2)));
      EXPECT_FALSE(Exists(fields.o.GetCol("two")));
      EXPECT_FALSE(Exists(fields.o.Get(2, "too")));
      EXPECT_TRUE(Exists(fields.o.GetCol("too")));
      EXPECT_FALSE(Exists(fields.o.GetCol("three")));
      EXPECT_FALSE(fields.o.Cols().Has("three"));
      EXPECT_TRUE(fields.o.Cols().Has("too"));
    }).Wait();
  }
}

namespace transactional_storage_test {

struct CurrentStorageTestMagicTypesExtractor {
  std::string& s;
  CurrentStorageTestMagicTypesExtractor(std::string& s) : s(s) {}
  template <typename CONTAINER_TYPE_WRAPPER, typename ENTRY_TYPE_WRAPPER>
  int operator()(const char* name, CONTAINER_TYPE_WRAPPER, ENTRY_TYPE_WRAPPER) {
    s = std::string(name) + ", " + CONTAINER_TYPE_WRAPPER::HumanReadableName() + ", " +
        current::reflection::CurrentTypeName<typename ENTRY_TYPE_WRAPPER::T_ENTRY>();
    return 42;  // Checked against via `::current::storage::FieldNameAndTypeByIndexAndReturn`.
  }
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, FieldAccessors) {
  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  EXPECT_EQ(3u, Storage::FIELDS_COUNT);
  Storage storage;
  EXPECT_EQ("d", storage(::current::storage::FieldNameByIndex<0>()));
  EXPECT_EQ("m", storage(::current::storage::FieldNameByIndex<1>()));
  EXPECT_EQ("o", storage(::current::storage::FieldNameByIndex<2>()));

  {
    std::string s;
    storage(::current::storage::FieldNameAndTypeByIndex<0>(), CurrentStorageTestMagicTypesExtractor(s));
    EXPECT_EQ("d, OrderedDictionary, Record", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<1, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("m, UnorderedMatrix, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<2, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("o, UnorderedOne2One, Cell", s);
  }
}

TEST(TransactionalStorage, Exceptions) {
  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;

  bool should_throw;
  const auto f_void = [&should_throw](ImmutableFields<Storage>) {
    if (should_throw) {
      CURRENT_STORAGE_THROW_ROLLBACK();
    }
  };

  const auto f_void_custom_exception = [&should_throw](ImmutableFields<Storage>) {
    if (should_throw) {
      throw std::logic_error("wtf");
    }
  };

  bool throw_with_value;
  const auto f_int = [&should_throw, &throw_with_value](ImmutableFields<Storage>) {
    if (should_throw) {
      if (throw_with_value) {
        CURRENT_STORAGE_THROW_ROLLBACK_WITH_VALUE(int, -1);
      } else {
        CURRENT_STORAGE_THROW_ROLLBACK();
      }
    } else {
      return 42;
    }
  };

  const auto f_int_custom_exception = [&should_throw](ImmutableFields<Storage>) {
    if (should_throw) {
      throw 100500;
    } else {
      return 42;  // LCOV_EXCL_LINE
    }
  };

  // `void` lambda successfully returned.
  {
    should_throw = false;
    const auto result = storage.Transaction(f_void).Go();
    EXPECT_TRUE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
  }

  // `void` lambda with rollback requested by user.
  {
    should_throw = true;
    const auto result = storage.Transaction(f_void).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
  }

  // `void` lambda with exception.
  {
    should_throw = true;
    auto result = storage.Transaction(f_void_custom_exception);
    result.Wait();
    // We can ignore the exception until we try to get the result.
    EXPECT_THROW(result.Go(), std::logic_error);
  }

  // `int` lambda successfully returned.
  {
    should_throw = false;
    const auto result = storage.Transaction(f_int).Go();
    EXPECT_TRUE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
    EXPECT_EQ(42, Value(result));
  }

  // `int` lambda with rollback not returning the value requested by user.
  {
    should_throw = true;
    throw_with_value = false;
    const auto result = storage.Transaction(f_int).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_FALSE(Exists(result));
  }

  // `int` lambda with rollback returning the value requested by user.
  {
    should_throw = true;
    throw_with_value = true;
    const auto result = storage.Transaction(f_int).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
    EXPECT_EQ(-1, Value(result));
  }

  // `int` lambda with exception.
  {
    should_throw = true;
    bool was_thrown = false;
    try {
      storage.Transaction(f_int_custom_exception).Go();
    } catch (int e) {
      was_thrown = true;
      EXPECT_EQ(100500, e);
    }
    if (!was_thrown) {
      ASSERT_TRUE(false) << "`f_int_custom_exception` threw no exception.";
    }
  }
}

TEST(TransactionalStorage, ReplicationViaHTTP) {
  using namespace transactional_storage_test;
  using current::storage::TransactionMetaFields;
  using Storage = TestStorage<SherlockStreamPersister>;

  // Create master storage.
  const std::string golden_storage_file_name =
      current::FileSystem::JoinPath("golden", "transactions_to_replicate.json");
  const std::string master_storage_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data1");
  const auto master_storage_file_remover = current::FileSystem::ScopedRmFile(master_storage_file_name);
  Storage master_storage(master_storage_file_name);
  master_storage.ExposeRawLogViaHTTP(FLAGS_transactional_storage_test_port, "/raw_log");

  // Perform a couple of transactions.
  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = master_storage.Transaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"one", 1});
      fields.d.Add(Record{"two", 2});
      fields.SetTransactionMetaField("user", "dima");
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = master_storage.Transaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"three", 3});
      fields.d.Erase("two");
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  // Confirm empty transactions are not persisted.
  {
    current::time::SetNow(std::chrono::microseconds(301));
    master_storage.Transaction([](ImmutableFields<Storage>) {}).Wait();
  }
  {
    current::time::SetNow(std::chrono::microseconds(302));
    master_storage.Transaction([](MutableFields<Storage>) {}).Wait();
  }

  // Confirm the non-empty transactions have been persisted, while the empty ones have been skipped.
  EXPECT_EQ(current::FileSystem::ReadFileAsString(golden_storage_file_name),
            current::FileSystem::ReadFileAsString(master_storage_file_name));

  // Create storage for replication.
  const std::string replicated_storage_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data2");
  const auto replicated_storage_file_remover = current::FileSystem::ScopedRmFile(replicated_storage_file_name);
  Storage replicated_storage(replicated_storage_file_name);

  // Replicate data via subscription to master storage raw log.
  const auto response =
      HTTP(GET(Printf("http://localhost:%d/raw_log?cap=1000&nowait", FLAGS_transactional_storage_test_port)));
  EXPECT_EQ(200, static_cast<int>(response.code));
  std::istringstream body(response.body);
  std::string line;
  uint64_t expected_index = 0u;
  while (std::getline(body, line)) {
    const size_t tab_pos = line.find('\t');
    ASSERT_FALSE(tab_pos == std::string::npos);
    const auto idx_ts = ParseJSON<idxts_t>(line.substr(0, tab_pos));
    EXPECT_EQ(expected_index, idx_ts.index);
    auto transaction = ParseJSON<Storage::T_TRANSACTION>(line.substr(tab_pos + 1));
    ASSERT_NO_THROW(replicated_storage.ReplayTransaction(std::move(transaction), idx_ts));
    ++expected_index;
  }

  // Check that persisted files are the same.
  EXPECT_EQ(current::FileSystem::ReadFileAsString(master_storage_file_name),
            current::FileSystem::ReadFileAsString(replicated_storage_file_name));

  // Test data consistency performing a transaction in the replicated storage.
  replicated_storage.Transaction([](ImmutableFields<Storage> fields) {
    EXPECT_EQ(2u, fields.d.Size());
    EXPECT_EQ(1, Value(fields.d["one"]).rhs);
    EXPECT_EQ(3, Value(fields.d["three"]).rhs);
    EXPECT_FALSE(Exists(fields.d["two"]));
  }).Wait();
}

namespace transactional_storage_test {

template <typename T_SHERLOCK_ENTRY>
class StorageSherlockTestProcessorImpl {
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;

 public:
  StorageSherlockTestProcessorImpl(std::string& output) : output_(output) {}

  void SetAllowTerminate() { allow_terminate_ = true; }
  void SetAllowTerminateOnOnMoreEntriesOfRightType() {
    allow_terminate_on_no_more_entries_of_right_type_ = true;
  }

  EntryResponse operator()(const T_SHERLOCK_ENTRY& transaction, idxts_t current, idxts_t last) const {
    output_ += JSON(current) + '\t' + JSON(transaction) + '\n';
    if (current.index != last.index) {
      return EntryResponse::More;
    } else {
      allow_terminate_ = true;
      return EntryResponse::Done;
    }
  }

  TerminationResponse Terminate() const {
    if (allow_terminate_) {
      return TerminationResponse::Terminate;  // LCOV_EXCL_LINE
    } else {
      return TerminationResponse::Wait;
    }
  }

  EntryResponse EntryResponseIfNoMorePassTypeFilter() const {
    if (allow_terminate_on_no_more_entries_of_right_type_) {
      return EntryResponse::Done;  // LCOV_EXCL_LINE
    } else {
      return EntryResponse::More;
    }
  }

 private:
  mutable bool allow_terminate_ = false;
  bool allow_terminate_on_no_more_entries_of_right_type_ = false;
  std::string& output_;
};

template <typename E>
using StorageSherlockTestProcessor = current::ss::StreamSubscriber<StorageSherlockTestProcessorImpl<E>, E>;

}  // namespace transactional_storage_test

TEST(TransactionalStorage, InternalExposeStream) {
  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.Transaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"one", 1});
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = storage.Transaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"two", 2});
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string collected;
  StorageSherlockTestProcessor<Storage::T_TRANSACTION> processor(collected);
  storage.InternalExposeStream().Subscribe(processor).Join();
  EXPECT_EQ(
      "{\"index\":0,\"us\":100}\t{\"meta\":{\"timestamp\":100,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"data\":{\"lhs\":\"one\",\"rhs\":1}},\"\":\"T9205381019427680739\"}]}\n"
      "{\"index\":1,\"us\":200}\t{\"meta\":{\"timestamp\":200,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"data\":{\"lhs\":\"two\",\"rhs\":2}},\"\":\"T9205381019427680739\"}]}\n",
      collected);
}

TEST(TransactionalStorage, GracefulShutdown) {
  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  storage.GracefulShutdown();
  auto result = storage.Transaction([](ImmutableFields<Storage>) {});
  ASSERT_THROW(result.Go(), current::storage::StorageInGracefulShutdownException);
};

namespace transactional_storage_test {

CURRENT_STRUCT(SimpleUser) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(name, std::string);
  CURRENT_CONSTRUCTOR(SimpleUser)(const std::string& key = "", const std::string& name = "")
      : key(key), name(name) {}
  void InitializeOwnKey() { key = current::ToString(std::hash<std::string>()(name)); }
};

CURRENT_STRUCT(SimplePost) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(text, std::string);
  CURRENT_CONSTRUCTOR(SimplePost)(const std::string& key = "", const std::string& text = "")
      : key(key), text(text) {}
  void InitializeOwnKey() { key = current::ToString(std::hash<std::string>()(text)); }  // LCOV_EXCL_LINE
};

CURRENT_STRUCT(SimpleLikeBase) {
  CURRENT_FIELD(row, std::string);
  CURRENT_FIELD(col, std::string);
  CURRENT_CONSTRUCTOR(SimpleLikeBase)(const std::string& who = "", const std::string& what = "")
      : row(who), col(what) {}
};

CURRENT_STRUCT(SimpleLike, SimpleLikeBase) {
  using T_BRIEF = SUPER;
  CURRENT_FIELD(details, Optional<std::string>);
  CURRENT_CONSTRUCTOR(SimpleLike)(const std::string& who = "", const std::string& what = "")
      : SUPER(who, what) {}
  CURRENT_CONSTRUCTOR(SimpleLike)(const std::string& who, const std::string& what, const std::string& details)
      : SUPER(who, what), details(details) {}
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, SimpleUser, SimpleUserPersisted);  // Ordered for list view.
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, SimplePost, SimplePostPersisted);
// TODO(dkorolev): Ordered matrix too.
CURRENT_STORAGE_FIELD_ENTRY(UnorderedMatrix, SimpleLike, SimpleLikePersisted);

CURRENT_STORAGE(SimpleStorage) {
  CURRENT_STORAGE_FIELD(user, SimpleUserPersisted);
  CURRENT_STORAGE_FIELD(post, SimplePostPersisted);
  CURRENT_STORAGE_FIELD(like, SimpleLikePersisted);
};

}  // namespace transactional_storage_test

// RESTful API test.
TEST(TransactionalStorage, RESTfulAPITest) {
  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using Storage = SimpleStorage<JSONFilePersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  EXPECT_EQ(3u, Storage::FIELDS_COUNT);
  Storage storage(persistence_file_name);

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_transactional_storage_test_port);

  // Run twice to make sure the `GET-POST-GET-DELETE` cycle is complete.
  for (size_t i = 0; i < 2; ++i) {
    // Register RESTful HTTP endpoints, in a scoped way.
    auto rest = RESTfulStorage<Storage>(
        storage, FLAGS_transactional_storage_test_port, "/api", "http://unittest.current.ai");
    rest.RegisterAlias("user", "user_alias");

    // Confirm an empty collection is returned.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/user")).code));
    EXPECT_EQ("", HTTP(GET(base_url + "/api/data/user")).body);

    const auto post_user1_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("max", "MZ")));
    EXPECT_EQ(201, static_cast<int>(post_user1_response.code));
    const auto user1_key = post_user1_response.body;
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/user/" + user1_key)).code));
    EXPECT_EQ("MZ", ParseJSON<SimpleUser>(HTTP(GET(base_url + "/api/data/user/" + user1_key)).body).name);

    // Test the alias too.
    EXPECT_EQ("MZ", ParseJSON<SimpleUser>(HTTP(GET(base_url + "/api/data/user_alias/" + user1_key)).body).name);

    // Test other key format too.
    EXPECT_EQ("MZ", ParseJSON<SimpleUser>(HTTP(GET(base_url + "/api/data/user?key=" + user1_key)).body).name);

    // Confirm a collection of one element is returned.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/user")).code));
    EXPECT_EQ(user1_key + '\n', HTTP(GET(base_url + "/api/data/user")).body);

    // Test collection retrieval.
    const auto post_user2_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("dima", "DK")));
    EXPECT_EQ(201, static_cast<int>(post_user2_response.code));
    const auto user2_key = post_user2_response.body;
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/user")).code));
    EXPECT_EQ(user1_key + '\n' + user2_key + '\n', HTTP(GET(base_url + "/api/data/user")).body);

    // Delete the users.
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api/data/user/" + user1_key)).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api/data/user/" + user2_key)).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/user/max")).code));

    // Run the subset of the above test for posts, not just for users.
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/post/test")).code));

    EXPECT_EQ(201,
              static_cast<int>(HTTP(PUT(base_url + "/api/data/post/test", SimplePost("test", "blah"))).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/post/test")).code));
    EXPECT_EQ("blah", ParseJSON<SimplePost>(HTTP(GET(base_url + "/api/data/post/test")).body).text);

    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api/data/post/test")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/post/test")).code));

    // Test RESTful matrix too.
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/like/dima-beer")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/like/max-beer")).code));

    // For this test, we disallow POST for the `/like`-s matrix.
    EXPECT_EQ(405, static_cast<int>(HTTP(POST(base_url + "/api/data/like", SimpleLike("dima", "beer"))).code));

    // Add a few likes.
    EXPECT_EQ(
        201,
        static_cast<int>(HTTP(PUT(base_url + "/api/data/like/dima-beer", SimpleLike("dima", "beer"))).code));
    EXPECT_EQ(201,
              static_cast<int>(
                  HTTP(PUT(base_url + "/api/data/like/max-beer", SimpleLike("max", "beer", "Cheers!"))).code));

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/like/dima-beer")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/like/max-beer")).code));

    EXPECT_EQ("{\"row\":\"dima\",\"col\":\"beer\",\"details\":null}\n",
              HTTP(GET(base_url + "/api/data/like/dima-beer")).body);
    // Meh, this test is not for `AdvancedHypermedia`. -- D.K.
    // EXPECT_EQ("{\"row\":\"max\",\"col\":\"beer\"}\n",
    //           HTTP(GET(base_url + "/api/data/like/max-beer?fields=brief")).body);
    EXPECT_EQ("{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}\n",
              HTTP(GET(base_url + "/api/data/like/max-beer")).body);

    // GET matrix as the collection.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/data/like")).code));
    EXPECT_EQ("max:beer\ndima:beer\n", HTTP(GET(base_url + "/api/data/like")).body);

    // Clean up the likes.
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api/data/like/dima-beer")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api/data/like/max-beer")).code));

    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/like/dima-beer")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/like/max-beer")).code));

    // Confirm REST endpoints successfully change to 503s.
    rest.SwitchHTTPEndpointsTo503s();
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api/data/post", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api/data/post/test", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api/data/like/blah-blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api/data/like", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api/data/like/blah-blah", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api/data/like/blah-blah")).code));

    // Twice, just in case.
    rest.SwitchHTTPEndpointsTo503s();
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api/data/post", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api/data/post/test", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api/data/like/blah-blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api/data/like", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api/data/like/blah-blah", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api/data/like/blah-blah")).code));
  }

  const std::vector<std::string> persisted_transactions = current::strings::Split<current::strings::ByLines>(
      current::FileSystem::ReadFileAsString(persistence_file_name));

  EXPECT_EQ(20u, persisted_transactions.size());
}

// LCOV_EXCL_START
// Test the `CURRENT_STORAGE_FIELD_EXCLUDE_FROM_REST(field)` macro.
namespace transactional_storage_test {
CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, SimpleUser, SimpleUserPersistedExposed);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, SimplePost, SimplePostPersistedNotExposed);
CURRENT_STORAGE(PartiallyExposedStorage) {
  CURRENT_STORAGE_FIELD(user, SimpleUserPersistedExposed);
  CURRENT_STORAGE_FIELD(post, SimplePostPersistedNotExposed);
  CURRENT_STORAGE_FIELD(like, SimpleLikePersisted);
};
}  // namespace transactional_storage_test
// LCOV_EXCL_STOP

CURRENT_STORAGE_FIELD_EXCLUDE_FROM_REST(transactional_storage_test::SimplePostPersistedNotExposed);

TEST(TransactionalStorage, RESTfulAPIDoesNotExposeHiddenFieldsTest) {
  using namespace transactional_storage_test;
  using namespace current::storage::rest;

  using Storage1 = SimpleStorage<SherlockInMemoryStreamPersister>;
  using Storage2 = PartiallyExposedStorage<SherlockInMemoryStreamPersister>;

  EXPECT_EQ(3u, Storage1::FIELDS_COUNT);
  EXPECT_EQ(3u, Storage2::FIELDS_COUNT);

  Storage1 storage1;
  Storage2 storage2;

  static_assert(current::storage::rest::FieldExposedViaREST<Storage1, SimpleUserPersisted>::exposed, "");
  static_assert(current::storage::rest::FieldExposedViaREST<Storage1, SimplePostPersisted>::exposed, "");

  static_assert(current::storage::rest::FieldExposedViaREST<Storage2, SimpleUserPersistedExposed>::exposed, "");
  static_assert(!current::storage::rest::FieldExposedViaREST<Storage2, SimplePostPersistedNotExposed>::exposed,
                "");

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_transactional_storage_test_port);

  auto rest1 = RESTfulStorage<Storage1, current::storage::rest::Hypermedia>(
      storage1, FLAGS_transactional_storage_test_port, "/api1", "http://unittest.current.ai/api1");
  auto rest2 = RESTfulStorage<Storage2, current::storage::rest::Hypermedia>(
      storage2, FLAGS_transactional_storage_test_port, "/api2", "http://unittest.current.ai/api2");

  const auto fields1 = ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api1")).body);
  const auto fields2 = ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api2")).body);

  EXPECT_TRUE(fields1.url_data.count("user") == 1);
  EXPECT_TRUE(fields1.url_data.count("post") == 1);
  EXPECT_TRUE(fields1.url_data.count("like") == 1);
  EXPECT_EQ(3u, fields1.url_data.size());

  EXPECT_TRUE(fields2.url_data.count("user") == 1);
  EXPECT_TRUE(fields2.url_data.count("post") == 0);
  EXPECT_TRUE(fields2.url_data.count("like") == 1);
  EXPECT_EQ(2u, fields2.url_data.size());

  // Confirms the status returns proper URL prefix.
  EXPECT_EQ("http://unittest.current.ai/api1", fields1.url);
  EXPECT_EQ("http://unittest.current.ai/api1/status", fields1.url_status);

  EXPECT_EQ("http://unittest.current.ai/api2", fields2.url);
  EXPECT_EQ("http://unittest.current.ai/api2/status", fields2.url_status);
}

TEST(TransactionalStorage, ShuttingDownAPIReportsUpAsFalse) {
  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using Storage = SimpleStorage<SherlockInMemoryStreamPersister>;

  Storage storage;

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_transactional_storage_test_port);

  auto rest = RESTfulStorage<Storage, current::storage::rest::Hypermedia>(
      storage, FLAGS_transactional_storage_test_port, "/api", "http://unittest.current.ai");

  EXPECT_TRUE(ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api")).body).up);
  EXPECT_TRUE(ParseJSON<HypermediaRESTStatus>(HTTP(GET(base_url + "/api/status")).body).up);
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/post/foo")).code));

  rest.SwitchHTTPEndpointsTo503s();

  EXPECT_FALSE(ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api")).body).up);
  EXPECT_FALSE(ParseJSON<HypermediaRESTStatus>(HTTP(GET(base_url + "/api/status")).body).up);
  EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api/data/post/foo")).code));
}

TEST(TransactionalStorage, UseExternallyProvidedSherlockStream) {
  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  static_assert(std::is_same<typename Storage::T_PERSISTER::T_SHERLOCK,
                             current::sherlock::Stream<typename Storage::T_PERSISTER::T_TRANSACTION,
                                                       current::persistence::Memory>>::value,
                "");

  typename Storage::T_PERSISTER::T_SHERLOCK stream;
  Storage storage(stream);

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.Transaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"own_stream", 42});
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string collected;
  StorageSherlockTestProcessor<Storage::T_TRANSACTION> processor(collected);
  storage.InternalExposeStream().Subscribe(processor).Join();
  EXPECT_EQ(
      "{\"index\":0,\"us\":100}\t{\"meta\":{\"timestamp\":100,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"data\":{\"lhs\":\"own_stream\",\"rhs\":42}},\"\":"
      "\"T9205381019427680739\"}]}\n",
      collected);
}

namespace transactional_storage_test {

CURRENT_STRUCT(StreamEntryOutsideStorage) {
  CURRENT_FIELD(s, std::string);
  CURRENT_CONSTRUCTOR(StreamEntryOutsideStorage)(const std::string& s = "") : s(s) {}
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, UseExternallyProvidedSherlockStreamOfBroaderType) {
  using namespace transactional_storage_test;
  using storage_t = TestStorage<SherlockInMemoryStreamPersister>;
  using transaction_t = typename storage_t::T_PERSISTER::T_TRANSACTION;

  static_assert(std::is_same<transaction_t, typename storage_t::T_TRANSACTION>::value, "");

  using Storage = TestStorage<SherlockInMemoryStreamPersister,
                              current::storage::transaction_policy::Synchronous,
                              Variant<transaction_t, StreamEntryOutsideStorage>>;

  static_assert(std::is_same<typename Storage::T_PERSISTER::T_SHERLOCK,
                             current::sherlock::Stream<Variant<transaction_t, StreamEntryOutsideStorage>,
                                                       current::persistence::Memory>>::value,
                "");

  typename Storage::T_PERSISTER::T_SHERLOCK stream;

  Storage storage(stream);

  {
    // Add three records to the stream: first and third externally, second through the storage.
    { stream.Publish(StreamEntryOutsideStorage("one"), std::chrono::microseconds(1)); }

    {
      current::time::SetNow(std::chrono::microseconds(2));
      const auto result = storage.Transaction([](MutableFields<Storage> fields) {
        fields.d.Add(Record{"two", 2});
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }
    { stream.Publish(StreamEntryOutsideStorage("three"), std::chrono::microseconds(3)); }
  }

  {
    // Subscribe to and collect transactions.
    std::string collected_transactions;
    StorageSherlockTestProcessor<transaction_t> processor(collected_transactions);
    processor.SetAllowTerminateOnOnMoreEntriesOfRightType();
    storage.InternalExposeStream().Subscribe<transaction_t>(processor).Join();
    EXPECT_EQ(
        "{\"index\":1,\"us\":2}\t"
        "{\"meta\":{\"timestamp\":2,\"fields\":{}},\"mutations\":["
        "{\"RecordDictionaryUpdated\":{\"data\":{\"lhs\":\"two\",\"rhs\":2}},\"\":\"T9205381019427680739\"}"
        "]}\n",
        collected_transactions);
  }

  {
    // Subscribe to and collect non-transactions.
    std::string collected_non_transactions;
    StorageSherlockTestProcessor<StreamEntryOutsideStorage> processor(collected_non_transactions);
    processor.SetAllowTerminateOnOnMoreEntriesOfRightType();
    storage.InternalExposeStream().Subscribe<StreamEntryOutsideStorage>(processor).Join();
    EXPECT_EQ("{\"index\":0,\"us\":1}\t{\"s\":\"one\"}\n{\"index\":2,\"us\":3}\t{\"s\":\"three\"}\n",
              collected_non_transactions);
  }

  {
    // Confirm replaying storage with a mixed-content stream does its job.
    Storage replayed(stream);
    const auto result = replayed.Transaction([](ImmutableFields<Storage> fields) {
      EXPECT_EQ(1u, fields.d.Size());
      ASSERT_TRUE(Exists(fields.d["two"]));
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}
