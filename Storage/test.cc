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

#define CURRENT_MOCK_TIME

#include <set>

#include "docu/docu_2_code.cc"
#include "docu/docu_3_code.cc"

#include "storage.h"
#include "api.h"
#include "persister/sherlock.h"

#include "rest/hypermedia.h"

#include "../Blocks/HTTP/api.h"

#include "../Bricks/file/file.h"
#include "../Bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

#ifndef CURRENT_WINDOWS
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
CURRENT_STORAGE_FIELD_ENTRY(UnorderedManyToUnorderedMany, Cell, CellUnorderedManyToUnorderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedManyToOrderedMany, Cell, CellOrderedManyToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedManyToOrderedMany, Cell, CellUnorderedManyToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedManyToUnorderedMany, Cell, CellOrderedManyToUnorderedMany);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToUnorderedOne, Cell, CellUnorderedOneToUnorderedOne);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToOrderedOne, Cell, CellOrderedOneToOrderedOne);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToOrderedOne, Cell, CellUnorderedOneToOrderedOne);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToUnorderedOne, Cell, CellOrderedOneToUnorderedOne);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToUnorderedMany, Cell, CellUnorderedOneToUnorderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToOrderedMany, Cell, CellOrderedOneToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToOrderedMany, Cell, CellUnorderedOneToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToUnorderedMany, Cell, CellOrderedOneToUnorderedMany);

CURRENT_STORAGE(TestStorage) {
  CURRENT_STORAGE_FIELD(d, RecordDictionary);
  CURRENT_STORAGE_FIELD(umany_to_umany, CellUnorderedManyToUnorderedMany);
  CURRENT_STORAGE_FIELD(omany_to_omany, CellOrderedManyToOrderedMany);
  CURRENT_STORAGE_FIELD(umany_to_omany, CellUnorderedManyToOrderedMany);
  CURRENT_STORAGE_FIELD(omany_to_umany, CellOrderedManyToUnorderedMany);
  CURRENT_STORAGE_FIELD(uone_to_uone, CellUnorderedOneToUnorderedOne);
  CURRENT_STORAGE_FIELD(oone_to_oone, CellOrderedOneToOrderedOne);
  CURRENT_STORAGE_FIELD(uone_to_oone, CellUnorderedOneToOrderedOne);
  CURRENT_STORAGE_FIELD(oone_to_uone, CellOrderedOneToUnorderedOne);
  CURRENT_STORAGE_FIELD(uone_to_umany, CellUnorderedOneToUnorderedMany);
  CURRENT_STORAGE_FIELD(oone_to_omany, CellOrderedOneToOrderedMany);
  CURRENT_STORAGE_FIELD(uone_to_omany, CellUnorderedOneToOrderedMany);
  CURRENT_STORAGE_FIELD(oone_to_umany, CellOrderedOneToUnorderedMany);
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, SmokeTest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<JSONFilePersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    EXPECT_EQ(13u, Storage::FIELDS_COUNT);
    Storage storage(persistence_file_name);

    // Fill a `Dictionary` container.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
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

    // Fill a `ManyToMany` container.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.umany_to_umany.Empty());
        EXPECT_EQ(0u, fields.umany_to_umany.Size());
        EXPECT_TRUE(fields.umany_to_umany.Rows().Empty());
        EXPECT_TRUE(fields.umany_to_umany.Cols().Empty());
        EXPECT_EQ(0u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(0u, fields.umany_to_umany.Cols().Size());
        fields.umany_to_umany.Add(Cell{1, "one", 1});
        fields.umany_to_umany.Add(Cell{2, "two", 2});
        fields.umany_to_umany.Add(Cell{2, "too", 3});
        EXPECT_FALSE(fields.umany_to_umany.Empty());
        EXPECT_EQ(3u, fields.umany_to_umany.Size());
        EXPECT_FALSE(fields.umany_to_umany.Rows().Empty());
        EXPECT_FALSE(fields.umany_to_umany.Cols().Empty());
        EXPECT_EQ(2u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(3u, fields.umany_to_umany.Cols().Size());
        EXPECT_EQ(2u, fields.umany_to_umany.Row(2).Size());
        EXPECT_EQ(1u, fields.umany_to_umany.Col("too").Size());
        EXPECT_TRUE(fields.umany_to_umany.Rows().Has(1));
        EXPECT_TRUE(fields.umany_to_umany.Row(3).Empty());
        EXPECT_TRUE(fields.umany_to_umany.Cols().Has("one"));
        EXPECT_TRUE(Exists(fields.umany_to_umany.Get(1, "one")));
        EXPECT_EQ(1, Value(fields.umany_to_umany.Get(1, "one")).phew);
        EXPECT_EQ(2, Value(fields.umany_to_umany.Get(2, "two")).phew);
        EXPECT_EQ(3, Value(fields.umany_to_umany.Get(2, "too")).phew);
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }

    // Fill a `OneToOne` container.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.uone_to_uone.Empty());
        EXPECT_EQ(0u, fields.uone_to_uone.Size());
        EXPECT_TRUE(fields.uone_to_uone.Rows().Empty());
        EXPECT_TRUE(fields.uone_to_uone.Cols().Empty());
        EXPECT_EQ(0u, fields.uone_to_uone.Rows().Size());
        EXPECT_EQ(0u, fields.uone_to_uone.Cols().Size());
        fields.uone_to_uone.Add(Cell{1, "one", 1});  // Adds {1,one=1}
        fields.uone_to_uone.Add(Cell{2, "two", 2});  // Adds {2,two=2}
        fields.uone_to_uone.Add(Cell{2, "too", 3});  // Adds {2,too=3}, removes {2,two=2}
        fields.uone_to_uone.Add(Cell{3, "too", 6});  // Adds {3,too=6}, removes {2,too=3}
        fields.uone_to_uone.Add(Cell{3, "too", 4});  // Adds {3,too=4}, overwrites {3,too=6}
        fields.uone_to_uone.Add(Cell{4, "fiv", 5});  // Adds {4,fiv=5}
        EXPECT_FALSE(fields.uone_to_uone.Empty());
        EXPECT_EQ(3u, fields.uone_to_uone.Size());
        EXPECT_FALSE(fields.uone_to_uone.Rows().Empty());
        EXPECT_FALSE(fields.uone_to_uone.Cols().Empty());
        EXPECT_EQ(3u, fields.uone_to_uone.Rows().Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Cols().Size());
        EXPECT_TRUE(fields.uone_to_uone.Rows().Has(1));
        EXPECT_TRUE(Exists(fields.uone_to_uone.GetEntryFromRow(1)));
        EXPECT_TRUE(fields.uone_to_uone.Cols().Has("one"));
        EXPECT_TRUE(Exists(fields.uone_to_uone.GetEntryFromCol("one")));
        EXPECT_FALSE(fields.uone_to_uone.Rows().Has(2));
        EXPECT_FALSE(Exists(fields.uone_to_uone.GetEntryFromRow(2)));
        EXPECT_FALSE(fields.uone_to_uone.Cols().Has("two"));
        EXPECT_FALSE(Exists(fields.uone_to_uone.GetEntryFromCol("two")));
        EXPECT_TRUE(Exists(fields.uone_to_uone.Get(3, "too")));
        EXPECT_FALSE(Exists(fields.uone_to_uone.Get(2, "too")));
        EXPECT_EQ(1, Value(fields.uone_to_uone.Get(1, "one")).phew);
        EXPECT_EQ(1, Value(fields.uone_to_uone.GetEntryFromRow(1)).phew);
        EXPECT_EQ(4, Value(fields.uone_to_uone.Get(3, "too")).phew);
        EXPECT_EQ(4, Value(fields.uone_to_uone.GetEntryFromCol("too")).phew);
        EXPECT_EQ(5, Value(fields.uone_to_uone.Get(4, "fiv")).phew);
        EXPECT_EQ(5, Value(fields.uone_to_uone.GetEntryFromRow(4)).phew);
        EXPECT_TRUE(fields.uone_to_uone.DoesNotConflict(2, "two"));
        EXPECT_FALSE(fields.uone_to_uone.DoesNotConflict(1, "three"));
        EXPECT_FALSE(fields.uone_to_uone.DoesNotConflict(4, "one"));
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }

    // Fill a `OneToMany` container.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.uone_to_umany.Empty());
        EXPECT_EQ(0u, fields.uone_to_umany.Size());
        EXPECT_TRUE(fields.uone_to_umany.Rows().Empty());
        EXPECT_TRUE(fields.uone_to_umany.Cols().Empty());
        EXPECT_EQ(0u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(0u, fields.uone_to_umany.Cols().Size());
        fields.uone_to_umany.Add(Cell{1, "one", 1});   // Adds {1,one=1 }
        fields.uone_to_umany.Add(Cell{2, "two", 5});   // Adds {2,two=5 }
        fields.uone_to_umany.Add(Cell{2, "two", 4});   // Adds {2,two=4 }, overwrites {2,two=5}
        fields.uone_to_umany.Add(Cell{1, "fiv", 5});   // Adds {1,fiv=5 }
        fields.uone_to_umany.Add(Cell{2, "fiv", 10});  // Adds {2,fiv=10}, removes {1,fiv=5}
        fields.uone_to_umany.Add(Cell{3, "six", 18});  // Adds {3,six=18}
        fields.uone_to_umany.Add(Cell{1, "six", 6});   // Adds {1,six=6 }, removes {3,six=18}
        EXPECT_FALSE(fields.uone_to_umany.Empty());
        EXPECT_EQ(4u, fields.uone_to_umany.Size());
        EXPECT_FALSE(fields.uone_to_umany.Rows().Empty());
        EXPECT_FALSE(fields.uone_to_umany.Cols().Empty());
        EXPECT_EQ(2u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(4u, fields.uone_to_umany.Cols().Size());
        EXPECT_EQ(2u, fields.uone_to_umany.Row(1).Size());
        EXPECT_TRUE(fields.uone_to_umany.Rows().Has(1));
        EXPECT_FALSE(fields.uone_to_umany.Rows().Has(3));
        EXPECT_TRUE(fields.uone_to_umany.Row(3).Empty());
        EXPECT_TRUE(fields.uone_to_umany.Cols().Has("two"));
        EXPECT_FALSE(fields.uone_to_umany.Cols().Has("too"));
        EXPECT_TRUE(Exists(fields.uone_to_umany.GetEntryFromCol("two")));
        EXPECT_FALSE(Exists(fields.uone_to_umany.GetEntryFromCol("tre")));
        EXPECT_TRUE(Exists(fields.uone_to_umany.Get(2, "fiv")));
        EXPECT_FALSE(Exists(fields.uone_to_umany.Get(3, "six")));
        EXPECT_EQ(1, Value(fields.uone_to_umany.Get(1, "one")).phew);
        EXPECT_EQ(4, Value(fields.uone_to_umany.Get(2, "two")).phew);
        EXPECT_EQ(10, Value(fields.uone_to_umany.Get(2, "fiv")).phew);
        EXPECT_EQ(6, Value(fields.uone_to_umany.Get(1, "six")).phew);
        EXPECT_TRUE(fields.uone_to_umany.DoesNotConflict(1, "too"));
        EXPECT_FALSE(fields.uone_to_umany.DoesNotConflict(2, "two"));
        EXPECT_FALSE(fields.uone_to_umany.DoesNotConflict(3, "six"));
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }

    // Copy data from unordered `ManyToMany`, `OneToOne` and `OneToMany` to corresponding ordered ones
    {
      const auto result1 = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.umany_to_umany.Empty());
        EXPECT_TRUE(fields.omany_to_omany.Empty());
        for (const auto& element : fields.umany_to_umany) {
          fields.omany_to_omany.Add(element);
          fields.umany_to_omany.Add(element);
          fields.omany_to_umany.Add(element);
        }
        EXPECT_EQ(fields.umany_to_umany.Size(), fields.omany_to_omany.Size());
      }).Go();
      EXPECT_TRUE(WasCommitted(result1));
      const auto result2 = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.uone_to_uone.Empty());
        EXPECT_TRUE(fields.oone_to_oone.Empty());
        for (const auto& element : fields.uone_to_uone) {
          fields.oone_to_oone.Add(element);
          fields.uone_to_oone.Add(element);
          fields.oone_to_uone.Add(element);
        }
        EXPECT_EQ(fields.uone_to_uone.Size(), fields.oone_to_oone.Size());
      }).Go();
      EXPECT_TRUE(WasCommitted(result2));
      const auto result3 = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_FALSE(fields.uone_to_umany.Empty());
        EXPECT_TRUE(fields.oone_to_omany.Empty());
        for (const auto& element : fields.uone_to_umany) {
          fields.oone_to_omany.Add(element);
          fields.uone_to_omany.Add(element);
          fields.oone_to_umany.Add(element);
        }
      }).Go();
      EXPECT_TRUE(WasCommitted(result3));
    }

    // Iterate over a `ManyToMany`, compare its ordered and unordered versions
    {
      const auto result1 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_FALSE(fields.umany_to_umany.Empty());
        EXPECT_FALSE(fields.omany_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto& row : fields.umany_to_umany.Rows()) {
          for (const auto& element : row) {
            data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                            current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                            current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& row : fields.omany_to_omany.Rows()) {
          for (const auto& element : row) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        std::multiset<int32_t> rows;
        for (const auto& row : fields.umany_to_omany.Rows()) {
          rows.insert(current::storage::sfinae::GetRow(*row.begin()));
        }
        for (const auto& row : rows) {
          for (const auto& element : fields.umany_to_omany.Row(row)) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto& row : fields.omany_to_umany.Rows()) {
          int32_t row_value = current::storage::sfinae::GetRow(*row.begin());
          std::multiset<std::string> cols;
          for (const auto& element : row) {
            cols.insert(current::storage::sfinae::GetCol(element));
          }
          for (const auto& col_value : cols) {
            data_vec.push_back(current::ToString(row_value) + ',' + current::ToString(col_value) + '=' +
                               current::ToString(Value(fields.omany_to_umany.Get(row_value, col_value)).phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data_vec, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result1));
      const auto result2 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_FALSE(fields.umany_to_umany.Empty());
        EXPECT_FALSE(fields.omany_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto& col : fields.umany_to_umany.Cols()) {
          for (const auto& element : col) {
            data_set.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                            current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                            current::ToString(element.phew));
          }
        }
        EXPECT_EQ("one,1=1 too,2=3 two,2=2", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& col : fields.omany_to_omany.Cols()) {
          for (const auto& element : col) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("one,1=1 too,2=3 two,2=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto& col : fields.umany_to_omany.Cols()) {
          std::string col_value = current::storage::sfinae::GetCol(*col.begin());
          std::multiset<int32_t> rows;
          for (const auto& element : col) {
            rows.insert(current::storage::sfinae::GetRow(element));
          }
          for (const auto& row_value : rows) {
            data_vec.push_back(current::ToString(col_value) + ',' + current::ToString(row_value) + '=' +
                               current::ToString(Value(fields.umany_to_omany.Get(row_value, col_value)).phew));
          }
        }
        EXPECT_EQ("one,1=1 too,2=3 two,2=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        std::multiset<std::string> cols;
        for (const auto& col : fields.omany_to_umany.Cols()) {
          cols.insert(current::storage::sfinae::GetCol(*col.begin()));
        }
        for (const auto& col : cols) {
          for (const auto& element : fields.omany_to_umany.Col(col)) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("one,1=1 too,2=3 two,2=2", current::strings::Join(data_vec, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result2));
      const auto result3 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        std::set<std::string> data_set;
        for (const auto& element : fields.umany_to_umany.Row(2)) {
          data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("2,too=3 2,two=2", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& element : fields.omany_to_omany.Row(2)) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("2,too=3 2,two=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto& element : fields.umany_to_omany.Row(2)) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("2,too=3 2,two=2", current::strings::Join(data_vec, ' '));
        data_set.clear();
        for (const auto& element : fields.omany_to_umany.Row(2)) {
          data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("2,too=3 2,two=2", current::strings::Join(data_set, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result3));
    }

    // Iterate over a `OneToOne`, compare its ordered and unordered versions.
    {
      const auto result1 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_FALSE(fields.uone_to_uone.Empty());
        EXPECT_FALSE(fields.oone_to_oone.Empty());
        std::multiset<std::string> data_set;
        for (const auto& element : fields.uone_to_uone.Rows()) {
          data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("1,one=1 3,too=4 4,fiv=5", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& element : fields.oone_to_oone.Rows()) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("1,one=1 3,too=4 4,fiv=5", current::strings::Join(data_vec, ' '));
        data_set.clear();
        for (const auto& element : fields.uone_to_oone.Rows()) {
          data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("1,one=1 3,too=4 4,fiv=5", current::strings::Join(data_set, ' '));
        data_vec.clear();
        for (const auto& element : fields.oone_to_uone.Rows()) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("1,one=1 3,too=4 4,fiv=5", current::strings::Join(data_vec, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result1));
      const auto result2 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_FALSE(fields.uone_to_uone.Empty());
        EXPECT_FALSE(fields.oone_to_oone.Empty());
        std::multiset<std::string> data_set;
        for (const auto& element : fields.uone_to_uone.Cols()) {
          data_set.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("fiv,4=5 one,1=1 too,3=4", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& element : fields.oone_to_oone.Cols()) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("fiv,4=5 one,1=1 too,3=4", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto& element : fields.uone_to_oone.Cols()) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("fiv,4=5 one,1=1 too,3=4", current::strings::Join(data_vec, ' '));
        data_set.clear();
        for (const auto& element : fields.oone_to_uone.Cols()) {
          data_set.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("fiv,4=5 one,1=1 too,3=4", current::strings::Join(data_set, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result2));
    }

    // Iterate over a `OneToMany`, compare its ordered and unordered versions.
    {
      const auto result1 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_FALSE(fields.uone_to_umany.Empty());
        EXPECT_FALSE(fields.oone_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto& row : fields.uone_to_umany.Rows()) {
          for (const auto& element : row) {
            data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                            current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                            current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 1,six=6 2,fiv=10 2,two=4", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& row : fields.oone_to_omany.Rows()) {
          for (const auto& element : row) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 1,six=6 2,fiv=10 2,two=4", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        std::multiset<int32_t> rows;
        for (const auto& row : fields.uone_to_omany.Rows()) {
          rows.insert(current::storage::sfinae::GetRow(*row.begin()));
        }
        for (const auto& row : rows) {
          for (const auto& element : fields.uone_to_omany.Row(row)) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 1,six=6 2,fiv=10 2,two=4", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto& row : fields.oone_to_umany.Rows()) {
          int32_t row_value = current::storage::sfinae::GetRow(*row.begin());
          std::multiset<std::string> cols;
          for (const auto& element : row) {
            cols.insert(current::storage::sfinae::GetCol(element));
          }
          for (const auto& col_value : cols) {
            data_vec.push_back(current::ToString(row_value) + ',' + current::ToString(col_value) + '=' +
                               current::ToString(Value(fields.oone_to_umany.Get(row_value, col_value)).phew));
          }
        }
        EXPECT_EQ("1,one=1 1,six=6 2,fiv=10 2,two=4", current::strings::Join(data_vec, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result1));
      const auto result2 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_FALSE(fields.uone_to_umany.Empty());
        EXPECT_FALSE(fields.oone_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto& element : fields.uone_to_umany.Cols()) {
          data_set.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("fiv,2=10 one,1=1 six,1=6 two,2=4", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& element : fields.oone_to_omany.Cols()) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                             current::ToString(element.phew));
        }
        data_vec.clear();
        for (const auto& element : fields.uone_to_omany.Cols()) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                             current::ToString(element.phew));
        }
        data_set.clear();
        EXPECT_EQ("fiv,2=10 one,1=1 six,1=6 two,2=4", current::strings::Join(data_vec, ' '));
        for (const auto& element : fields.oone_to_umany.Cols()) {
          data_set.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("fiv,2=10 one,1=1 six,1=6 two,2=4", current::strings::Join(data_set, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result2));
      const auto result3 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        std::set<std::string> data_set;
        for (const auto& element : fields.uone_to_umany.Row(2)) {
          data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("2,fiv=10 2,two=4", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto& element : fields.oone_to_omany.Row(2)) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("2,fiv=10 2,two=4", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto& element : fields.uone_to_omany.Row(2)) {
          data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                             current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                             current::ToString(element.phew));
        }
        EXPECT_EQ("2,fiv=10 2,two=4", current::strings::Join(data_vec, ' '));
        data_set.clear();
        for (const auto& element : fields.oone_to_umany.Row(2)) {
          data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                          current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                          current::ToString(element.phew));
        }
        EXPECT_EQ("2,fiv=10 2,two=4", current::strings::Join(data_set, ' '));
      }).Go();
      EXPECT_TRUE(WasCommitted(result3));
    }

    // Rollback a transaction involving a `ManyToMany`, `OneToOne` and `OneToMany`.
    {
      const auto result1 = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_EQ(3u, fields.umany_to_umany.Size());
        EXPECT_EQ(2u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(3u, fields.umany_to_umany.Cols().Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Rows().Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Cols().Size());
        EXPECT_EQ(4u, fields.uone_to_umany.Size());
        EXPECT_EQ(2u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(4u, fields.uone_to_umany.Cols().Size());

        fields.d.Add(Record{"one", 100});
        fields.d.Add(Record{"four", 4});
        fields.d.Erase("two");

        fields.umany_to_umany.Add(Cell{1, "one", 42});
        fields.umany_to_umany.Add(Cell{3, "three", 3});
        fields.umany_to_umany.Erase(2, "two");

        fields.uone_to_uone.Add(Cell{3, "too", 6});  // Adds {3,too=6}, overwrites {3,too=4}
        fields.uone_to_uone.Add(Cell{4, "for", 4});  // Adds {4,for=4}, removes {4,fiv=5}
        fields.uone_to_uone.Add(Cell{5, "fiv", 7});  // Adds {5,fiv=7}
        fields.uone_to_uone.EraseRow(1);

        fields.uone_to_umany.Add(Cell{2, "six", 7});  // Adds {2,six=7}, removes {1,six=6}
        fields.uone_to_umany.Add(Cell{2, "two", 5});  // Adds {2,two=5}, overwrites {2,two=4}
        fields.uone_to_umany.Add(Cell{4, "sev", 1});  // Adds {4,sev=1}
        fields.uone_to_umany.EraseCol("one");         // Removes {1,one=1}
        fields.uone_to_umany.Erase(2, "fiv");         // Removes {2,fiv=10}

        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go();
      EXPECT_FALSE(WasCommitted(result1));

      const auto result2 = storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        EXPECT_EQ(3u, fields.umany_to_umany.Size());
        EXPECT_EQ(2u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(3u, fields.umany_to_umany.Cols().Size());

        EXPECT_EQ(3u, fields.uone_to_uone.Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Rows().Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Cols().Size());
        EXPECT_FALSE(fields.uone_to_uone.Cols().Has("three"));
        EXPECT_TRUE(fields.uone_to_uone.Rows().Has(3));
        EXPECT_TRUE(fields.uone_to_uone.Cols().Has("one"));

        EXPECT_EQ(4u, fields.uone_to_umany.Size());
        EXPECT_EQ(2u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(4u, fields.uone_to_umany.Cols().Size());
        EXPECT_FALSE(fields.uone_to_umany.Rows().Has(4));
        EXPECT_FALSE(fields.uone_to_umany.Cols().Has("sev"));
        EXPECT_TRUE(fields.uone_to_umany.Rows().Has(1));
        EXPECT_TRUE(fields.uone_to_umany.Cols().Has("fiv"));
      }).Go();
      EXPECT_TRUE(WasCommitted(result2));
    }

    // Iterate over a `ManyToMany` with deleted elements, confirm the integrity of `forward_` and `transposed_`.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.umany_to_umany.Rows().Has(2));
        EXPECT_TRUE(fields.umany_to_umany.Cols().Has("two"));
        EXPECT_TRUE(fields.umany_to_umany.Cols().Has("too"));

        fields.umany_to_umany.Erase(2, "two");
        EXPECT_TRUE(fields.umany_to_umany.Rows().Has(2));  // Still has another "2" left, the {2, "too"} key.
        EXPECT_FALSE(fields.umany_to_umany.Cols().Has("two"));
        EXPECT_TRUE(fields.umany_to_umany.Cols().Has("too"));
        EXPECT_EQ(2u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(2u, fields.umany_to_umany.Cols().Size());

        fields.umany_to_umany.Erase(2, "too");
        EXPECT_FALSE(fields.umany_to_umany.Rows().Has(2));
        EXPECT_FALSE(fields.umany_to_umany.Cols().Has("two"));
        EXPECT_FALSE(fields.umany_to_umany.Cols().Has("too"));
        EXPECT_EQ(1u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(1u, fields.umany_to_umany.Cols().Size());

        std::multiset<std::string> data1;
        std::multiset<std::string> data2;
        for (const auto& row : fields.umany_to_umany.Rows()) {
          for (const auto& element : row) {
            data1.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                         current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                         current::ToString(element.phew));
          }
        }
        for (const auto& col : fields.umany_to_umany.Cols()) {
          for (const auto& element : col) {
            data2.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                         current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                         current::ToString(element.phew));
          }
        }

        EXPECT_EQ("1,one=1", current::strings::Join(data1, ' '));
        EXPECT_EQ("one,1=1", current::strings::Join(data2, ' '));

        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go();
      EXPECT_FALSE(WasCommitted(result));
    }

    // Iterate over a `OneToOne` with deleted elements, confirm the integrity of `forward_` and `transposed_`.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.uone_to_uone.Rows().Has(1));
        EXPECT_TRUE(fields.uone_to_uone.Cols().Has("one"));
        EXPECT_TRUE(fields.uone_to_uone.Cols().Has("too"));

        fields.uone_to_uone.Add(Cell{3, "two", 5});
        EXPECT_TRUE(fields.uone_to_uone.Rows().Has(3));
        EXPECT_FALSE(fields.uone_to_uone.Cols().Has("too"));
        EXPECT_TRUE(fields.uone_to_uone.Cols().Has("two"));
        EXPECT_EQ(3u, fields.uone_to_uone.Rows().Size());
        EXPECT_EQ(3u, fields.uone_to_uone.Cols().Size());

        fields.uone_to_uone.EraseCol("two");
        EXPECT_FALSE(fields.uone_to_uone.Rows().Has(3));
        EXPECT_FALSE(fields.uone_to_uone.Cols().Has("too"));
        EXPECT_FALSE(fields.uone_to_uone.Cols().Has("two"));
        EXPECT_EQ(2u, fields.uone_to_uone.Rows().Size());
        EXPECT_EQ(2u, fields.uone_to_uone.Cols().Size());

        std::multiset<std::string> rows;
        std::multiset<std::string> cols;
        for (const auto& element : fields.uone_to_uone.Rows()) {
          rows.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                      current::ToString(element.phew));
        }
        for (const auto& element : fields.uone_to_uone.Cols()) {
          cols.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                      current::ToString(element.phew));
        }

        EXPECT_EQ("1,one=1 4,fiv=5", current::strings::Join(rows, ' '));
        EXPECT_EQ("fiv,4=5 one,1=1", current::strings::Join(cols, ' '));

        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go();
      EXPECT_FALSE(WasCommitted(result));
    }

    // Iterate over a `OneToMany` with deleted elements, confirm the integrity of `forward_` and `transposed_`.
    {
      const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        EXPECT_TRUE(fields.uone_to_umany.Rows().Has(1));
        EXPECT_TRUE(fields.uone_to_umany.Cols().Has("six"));
        EXPECT_TRUE(fields.uone_to_umany.Cols().Has("one"));

        fields.uone_to_umany.Add(Cell{3, "one", 6});  // Adds {3,one=6}, removes {1,one=1}
        EXPECT_TRUE(fields.uone_to_umany.Rows().Has(3));
        EXPECT_TRUE(fields.uone_to_umany.Rows().Has(1));  // There is still {1,six=6}
        EXPECT_EQ(3u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(4u, fields.uone_to_umany.Cols().Size());

        fields.uone_to_umany.EraseCol("six");
        EXPECT_FALSE(fields.uone_to_umany.Rows().Has(1));
        EXPECT_FALSE(fields.uone_to_umany.Cols().Has("six"));
        EXPECT_EQ(2u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(3u, fields.uone_to_umany.Cols().Size());

        std::multiset<std::string> rows;
        std::multiset<std::string> cols;
        for (const auto& row : fields.uone_to_umany.Rows()) {
          for (const auto& element : row) {
            rows.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                        current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                        current::ToString(element.phew));
          }
        }
        for (const auto& element : fields.uone_to_umany.Cols()) {
          cols.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                      current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                      current::ToString(element.phew));
        }

        EXPECT_EQ("2,fiv=10 2,two=4 3,one=6", current::strings::Join(rows, ' '));
        EXPECT_EQ("fiv,2=10 one,3=6 two,2=4", current::strings::Join(cols, ' '));

        CURRENT_STORAGE_THROW_ROLLBACK();
      }).Go();
      EXPECT_FALSE(WasCommitted(result));
    }

    {
      const auto f = [](ImmutableFields<Storage>) { return 42; };

      const auto result = storage.ReadOnlyTransaction(f).Go();
      EXPECT_TRUE(WasCommitted(result));
      EXPECT_TRUE(Exists(result));
      EXPECT_EQ(42, Value(result));
    }
  }

  // Replay the entire storage from file.
  {
    Storage replayed(persistence_file_name);
    const auto result = replayed.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
      EXPECT_FALSE(fields.d.Empty());
      EXPECT_EQ(2u, fields.d.Size());
      EXPECT_EQ(1, Value(fields.d["one"]).rhs);
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);
      EXPECT_FALSE(Exists(fields.d["three"]));

      EXPECT_FALSE(fields.umany_to_umany.Empty());
      EXPECT_EQ(3u, fields.umany_to_umany.Size());
      EXPECT_EQ(1, Value(fields.umany_to_umany.Get(1, "one")).phew);
      EXPECT_EQ(2, Value(fields.umany_to_umany.Get(2, "two")).phew);
      EXPECT_EQ(3, Value(fields.umany_to_umany.Get(2, "too")).phew);
      EXPECT_FALSE(Exists(fields.umany_to_umany.Get(3, "three")));

      EXPECT_FALSE(fields.uone_to_uone.Empty());
      EXPECT_EQ(3u, fields.uone_to_uone.Size());
      EXPECT_EQ(1, Value(fields.uone_to_uone.Get(1, "one")).phew);
      EXPECT_EQ(1, Value(fields.uone_to_uone.GetEntryFromCol("one")).phew);
      EXPECT_EQ(4, Value(fields.uone_to_uone.Get(3, "too")).phew);
      EXPECT_EQ(4, Value(fields.uone_to_uone.GetEntryFromRow(3)).phew);
      EXPECT_EQ(5, Value(fields.uone_to_uone.Get(4, "fiv")).phew);
      EXPECT_EQ(5, Value(fields.uone_to_uone.GetEntryFromCol("fiv")).phew);
      EXPECT_FALSE(Exists(fields.uone_to_uone.Get(2, "two")));
      EXPECT_FALSE(Exists(fields.uone_to_uone.GetEntryFromRow(2)));
      EXPECT_FALSE(Exists(fields.uone_to_uone.GetEntryFromCol("two")));
      EXPECT_FALSE(Exists(fields.uone_to_uone.Get(2, "too")));
      EXPECT_TRUE(Exists(fields.uone_to_uone.GetEntryFromCol("too")));
      EXPECT_FALSE(Exists(fields.uone_to_uone.GetEntryFromCol("three")));
      EXPECT_TRUE(Exists(fields.uone_to_uone.GetEntryFromRow(4)));
      EXPECT_FALSE(fields.uone_to_uone.Cols().Has("three"));
      EXPECT_TRUE(fields.uone_to_uone.Cols().Has("too"));

      EXPECT_FALSE(fields.uone_to_umany.Empty());
      EXPECT_EQ(4u, fields.uone_to_umany.Size());
      EXPECT_EQ(1, Value(fields.uone_to_umany.Get(1, "one")).phew);
      EXPECT_EQ(1, Value(fields.uone_to_umany.GetEntryFromCol("one")).phew);
      EXPECT_EQ(4, Value(fields.uone_to_umany.Get(2, "two")).phew);
      EXPECT_EQ(4, Value(fields.uone_to_umany.GetEntryFromCol("two")).phew);
      EXPECT_EQ(6, Value(fields.uone_to_umany.Get(1, "six")).phew);
      EXPECT_EQ(6, Value(fields.uone_to_umany.GetEntryFromCol("six")).phew);
      EXPECT_EQ(10, Value(fields.uone_to_umany.Get(2, "fiv")).phew);
      EXPECT_EQ(10, Value(fields.uone_to_umany.GetEntryFromCol("fiv")).phew);
      EXPECT_FALSE(Exists(fields.uone_to_umany.Get(3, "six")));
      EXPECT_FALSE(Exists(fields.uone_to_umany.GetEntryFromCol("sev")));
      EXPECT_FALSE(Exists(fields.uone_to_umany.Get(4, "sev")));
      EXPECT_TRUE(Exists(fields.uone_to_umany.GetEntryFromCol("one")));
      EXPECT_FALSE(Exists(fields.uone_to_umany.GetEntryFromCol("too")));
      EXPECT_TRUE(Exists(fields.uone_to_umany.Get(2, "two")));
      EXPECT_FALSE(fields.uone_to_umany.Cols().Has("sev"));
      EXPECT_TRUE(fields.uone_to_umany.Cols().Has("fiv"));
      EXPECT_FALSE(fields.uone_to_umany.Rows().Has(3));
      EXPECT_TRUE(fields.uone_to_umany.Rows().Has(2));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}

namespace transactional_storage_test {

struct CurrentStorageTestMagicTypesExtractor {
  std::string& s;
  CurrentStorageTestMagicTypesExtractor(std::string& s) : s(s) {}
  template <typename CONTAINER_TYPE_WRAPPER, typename ENTRY_TYPE_WRAPPER>
  int operator()(const char* name, CONTAINER_TYPE_WRAPPER, ENTRY_TYPE_WRAPPER) {
    s = std::string(name) + ", " + CONTAINER_TYPE_WRAPPER::HumanReadableName() + ", " +
        current::reflection::CurrentTypeName<typename ENTRY_TYPE_WRAPPER::entry_t>();
    return 42;  // Checked against via `::current::storage::FieldNameAndTypeByIndexAndReturn`.
  }
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, FieldAccessors) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  EXPECT_EQ(13u, Storage::FIELDS_COUNT);
  Storage storage;
  EXPECT_EQ("d", storage(::current::storage::FieldNameByIndex<0>()));
  EXPECT_EQ("umany_to_umany", storage(::current::storage::FieldNameByIndex<1>()));
  EXPECT_EQ("omany_to_omany", storage(::current::storage::FieldNameByIndex<2>()));
  EXPECT_EQ("umany_to_omany", storage(::current::storage::FieldNameByIndex<3>()));
  EXPECT_EQ("omany_to_umany", storage(::current::storage::FieldNameByIndex<4>()));
  EXPECT_EQ("uone_to_uone", storage(::current::storage::FieldNameByIndex<5>()));
  EXPECT_EQ("oone_to_oone", storage(::current::storage::FieldNameByIndex<6>()));
  EXPECT_EQ("uone_to_oone", storage(::current::storage::FieldNameByIndex<7>()));
  EXPECT_EQ("oone_to_uone", storage(::current::storage::FieldNameByIndex<8>()));
  EXPECT_EQ("uone_to_umany", storage(::current::storage::FieldNameByIndex<9>()));
  EXPECT_EQ("oone_to_omany", storage(::current::storage::FieldNameByIndex<10>()));
  EXPECT_EQ("uone_to_omany", storage(::current::storage::FieldNameByIndex<11>()));
  EXPECT_EQ("oone_to_umany", storage(::current::storage::FieldNameByIndex<12>()));

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
    EXPECT_EQ("umany_to_umany, UnorderedManyToUnorderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<2, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("omany_to_omany, OrderedManyToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<3, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("umany_to_omany, UnorderedManyToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<4, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("omany_to_umany, OrderedManyToUnorderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<5, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_uone, UnorderedOneToUnorderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<6, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_oone, OrderedOneToOrderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<7, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_oone, UnorderedOneToOrderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<8, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_uone, OrderedOneToUnorderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<9, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_umany, UnorderedOneToUnorderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<10, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_omany, OrderedOneToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<11, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_omany, UnorderedOneToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              storage(::current::storage::FieldNameAndTypeByIndexAndReturn<12, int>(),
                      CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_umany, OrderedOneToUnorderedMany, Cell", s);
  }
}

TEST(TransactionalStorage, Exceptions) {
  current::time::ResetToZero();

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
    const auto result = storage.ReadOnlyTransaction(f_void).Go();
    EXPECT_TRUE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
  }

  // `void` lambda with rollback requested by user.
  {
    should_throw = true;
    const auto result = storage.ReadOnlyTransaction(f_void).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
  }

  // `void` lambda with exception.
  {
    should_throw = true;
    auto result = storage.ReadOnlyTransaction(f_void_custom_exception);
    result.Wait();
    // We can ignore the exception until we try to get the result.
    EXPECT_THROW(result.Go(), std::logic_error);
  }

  // `int` lambda successfully returned.
  {
    should_throw = false;
    const auto result = storage.ReadOnlyTransaction(f_int).Go();
    EXPECT_TRUE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
    EXPECT_EQ(42, Value(result));
  }

  // `int` lambda with rollback not returning the value requested by user.
  {
    should_throw = true;
    throw_with_value = false;
    const auto result = storage.ReadOnlyTransaction(f_int).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_FALSE(Exists(result));
  }

  // `int` lambda with rollback returning the value requested by user.
  {
    should_throw = true;
    throw_with_value = true;
    const auto result = storage.ReadOnlyTransaction(f_int).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
    EXPECT_EQ(-1, Value(result));
  }

  // `int` lambda with exception.
  {
    should_throw = true;
    bool was_thrown = false;
    try {
      storage.ReadOnlyTransaction(f_int_custom_exception).Go();
    } catch (int e) {
      was_thrown = true;
      EXPECT_EQ(100500, e);
    }
    if (!was_thrown) {
      ASSERT_TRUE(false) << "`f_int_custom_exception` threw no exception.";
    }
  }
}

TEST(TransactionalStorage, TransactionMetaFields) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  const auto& persister = storage.InternalExposeStream().InternalExposePersister();
  const auto TransactionByIndex = [&persister](uint64_t index) -> const Storage::transaction_t& {
    return (*persister.Iterate(index, index + 1).begin()).entry;
  };

  // Try to delete nonexistent entry, setting `who` transaction meta field.
  // No state should be changed as a result of executing the transaction.
  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.d.Erase("nonexistent");
      fields.SetTransactionMetaField("who", "anyone");
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  // The same for two-step transaction with user-initiated rollback.
  {
    current::time::SetNow(std::chrono::microseconds(200));
    bool second_step_executed = false;
    const auto result = storage.ReadWriteTransaction(
                                    [](MutableFields<Storage> fields) -> int {
                                      fields.d.Add(Record{"nonexistent", 0});
                                      fields.SetTransactionMetaField("where", "here");
                                      CURRENT_STORAGE_THROW_ROLLBACK_WITH_VALUE(int, 42);
                                      return 0;
                                    },
                                    [&second_step_executed](int v) {
                                      EXPECT_EQ(42, v);
                                      second_step_executed = true;
                                    }).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(second_step_executed);
  }

  // The same for two-step transaction with unhandled exception.
  {
    current::time::SetNow(std::chrono::microseconds(300));
    bool second_step_executed = false;
    auto future = storage.ReadWriteTransaction(
        [](MutableFields<Storage> fields) -> int {
          fields.d.Add(Record{"nonexistent2", 0});
          fields.SetTransactionMetaField("when", "now");
          throw current::Exception();
          return 0;
        },
        [&second_step_executed](int v) {
          EXPECT_EQ(0, v);
          second_step_executed = true;
        });
    EXPECT_THROW(future.Go(), current::Exception);
    EXPECT_FALSE(second_step_executed);
  }

  // Add one entry, setting another meta field - `why`.
  {
    current::time::SetNow(std::chrono::microseconds(1000));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"", 42});
      fields.SetTransactionMetaField("why", "because");
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  // Check that only the second transaction has been persisted with the sole meta field `why`.
  {
    ASSERT_EQ(1u, persister.Size());
    const auto& transaction = TransactionByIndex(0u);
    EXPECT_EQ(1000, transaction.meta.begin_us.count());
    EXPECT_EQ(1u, transaction.meta.fields.size());
    EXPECT_EQ(0u, transaction.meta.fields.count("who"));
    ASSERT_EQ(1u, transaction.meta.fields.count("why"));
    ASSERT_EQ("because", transaction.meta.fields.at("why"));
  }
}

TEST(TransactionalStorage, LastModifiedInDictionaryContainer) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  const auto& persister = storage.InternalExposeStream().InternalExposePersister();
  const auto TransactionByIndex = [&persister](uint64_t index) -> const Storage::transaction_t& {
    return (*persister.Iterate(index, index + 1).begin()).entry;
  };

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      ASSERT_FALSE(Exists(fields.d.LastModified("x")));
      current::time::SetNow(std::chrono::microseconds(101));
      fields.d.Add(Record{"x", 1});
      {
        const auto t = fields.d.LastModified("x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(101, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(102));
      fields.d.Add(Record{"y", 2});
      {
        const auto t = fields.d.LastModified("y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(102, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(103));
      fields.d.Erase("y");
      {
        const auto t = fields.d.LastModified("y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(103, Value(t).count());
      }
    }).Go();
    EXPECT_TRUE(WasCommitted(result));

    ASSERT_EQ(1u, persister.Size());
    const auto& transaction = TransactionByIndex(0u);
    EXPECT_EQ(100, transaction.meta.begin_us.count());
    EXPECT_EQ(103, transaction.meta.end_us.count());
    ASSERT_EQ(3u, transaction.mutations.size());
    ASSERT_TRUE(Exists<RecordDictionaryUpdated>(transaction.mutations[0]));
    EXPECT_EQ(101, Value<RecordDictionaryUpdated>(transaction.mutations[0]).us.count());
    ASSERT_TRUE(Exists<RecordDictionaryUpdated>(transaction.mutations[1]));
    EXPECT_EQ(102, Value<RecordDictionaryUpdated>(transaction.mutations[1]).us.count());
    ASSERT_TRUE(Exists<RecordDictionaryDeleted>(transaction.mutations[2]));
    EXPECT_EQ(103, Value<RecordDictionaryDeleted>(transaction.mutations[2]).us.count());
  }

  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"x", 100});
      fields.d.Erase("y");
      fields.d.Add(Record{"z", 3});
      CURRENT_STORAGE_THROW_ROLLBACK();
    }).Go();
    EXPECT_FALSE(WasCommitted(result));
  }

  {
    current::time::SetNow(std::chrono::microseconds(300));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      {
        const auto t = fields.d.LastModified("x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(101, Value(t).count());
      }
      {
        const auto t = fields.d.LastModified("y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(103, Value(t).count());
      }
      EXPECT_FALSE(Exists(fields.d.LastModified("z")));
      current::time::SetNow(std::chrono::microseconds(301));
      fields.d.Erase("x");
      {
        const auto t = fields.d.LastModified("x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(301, Value(t).count());
      }
    }).Go();
    EXPECT_TRUE(WasCommitted(result));

    ASSERT_EQ(2u, persister.Size());
    const auto& transaction = TransactionByIndex(1u);
    EXPECT_EQ(300, transaction.meta.begin_us.count());
    EXPECT_EQ(301, transaction.meta.end_us.count());
    ASSERT_EQ(1u, transaction.mutations.size());
    ASSERT_TRUE(Exists<RecordDictionaryDeleted>(transaction.mutations[0]));
    EXPECT_EQ(301, Value<RecordDictionaryDeleted>(transaction.mutations[0]).us.count());
  }
}

TEST(TransactionalStorage, LastModifiedInMatrixContainers) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  const auto& persister = storage.InternalExposeStream().InternalExposePersister();
  const auto TransactionByIndex = [&persister](uint64_t index) -> const Storage::transaction_t& {
    return (*persister.Iterate(index, index + 1).begin()).entry;
  };

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      ASSERT_FALSE(Exists(fields.umany_to_umany.LastModified(1, "x")));
      current::time::SetNow(std::chrono::microseconds(101));
      fields.umany_to_umany.Add(Cell{1, "x", 1});
      {
        const auto t = fields.umany_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(101, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(102));
      fields.uone_to_umany.Add(Cell{1, "x", 1});
      {
        const auto t = fields.uone_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(102, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(103));
      fields.uone_to_uone.Add(Cell{1, "x", 1});
      {
        const auto t = fields.uone_to_uone.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(103, Value(t).count());
      }
    }).Go();
    EXPECT_TRUE(WasCommitted(result));

    ASSERT_EQ(1u, persister.Size());
    const auto& transaction = TransactionByIndex(0u);
    EXPECT_EQ(100, transaction.meta.begin_us.count());
    EXPECT_EQ(103, transaction.meta.end_us.count());
    ASSERT_EQ(3u, transaction.mutations.size());
    ASSERT_TRUE(Exists<CellUnorderedManyToUnorderedManyUpdated>(transaction.mutations[0]));
    EXPECT_EQ(101, Value<CellUnorderedManyToUnorderedManyUpdated>(transaction.mutations[0]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[1]));
    EXPECT_EQ(102, Value<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[1]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[2]));
    EXPECT_EQ(103, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[2]).us.count());
  }

  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      EXPECT_FALSE(Exists(fields.umany_to_umany.LastModified(1, "y")));
      EXPECT_FALSE(Exists(fields.uone_to_umany.LastModified(1, "y")));
      EXPECT_FALSE(Exists(fields.uone_to_uone.LastModified(1, "y")));
      fields.umany_to_umany.Add(Cell{1, "x", 100});
      fields.uone_to_umany.Add(Cell{2, "x", 100});
      fields.uone_to_uone.Add(Cell{2, "y", 100});
      fields.uone_to_uone.Add(Cell{1, "y", 42});
      fields.umany_to_umany.Erase(1, "x");
      fields.uone_to_umany.Erase(2, "x");
      fields.uone_to_uone.Erase(1, "y");
      CURRENT_STORAGE_THROW_ROLLBACK();
    }).Go();
    EXPECT_FALSE(WasCommitted(result));
  }

  {
    current::time::SetNow(std::chrono::microseconds(300));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      {
        const auto t = fields.umany_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(101, Value(t).count());
      }
      {
        const auto t = fields.uone_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(102, Value(t).count());
      }
      {
        const auto t = fields.uone_to_uone.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(103, Value(t).count());
      }
      EXPECT_FALSE(Exists(fields.uone_to_umany.LastModified(2, "x")));
      EXPECT_FALSE(Exists(fields.uone_to_uone.LastModified(2, "y")));
      EXPECT_FALSE(Exists(fields.uone_to_uone.LastModified(1, "y")));

      current::time::SetNow(std::chrono::microseconds(301));
      fields.umany_to_umany.Add(Cell{1, "x", 100});
      {
        const auto t = fields.umany_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(301, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(302));
      fields.uone_to_umany.Add(Cell{2, "x", 100});
      {
        const auto t = fields.uone_to_umany.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(302, Value(t).count());
        const auto removed_t = fields.uone_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(removed_t));
        EXPECT_EQ(302, Value(removed_t).count());
      }
      current::time::SetNow(std::chrono::microseconds(303));
      fields.uone_to_uone.Add(Cell{2, "y", 100});
      {
        const auto t = fields.uone_to_uone.LastModified(2, "y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(303, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(304));
      fields.uone_to_uone.Add(Cell{1, "y", 42});
      {
        const auto t = fields.uone_to_uone.LastModified(1, "y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(304, Value(t).count());
        const auto removed_t1 = fields.uone_to_uone.LastModified(1, "x");
        ASSERT_TRUE(Exists(removed_t1));
        EXPECT_EQ(304, Value(removed_t1).count());
        const auto removed_t2 = fields.uone_to_uone.LastModified(2, "y");
        ASSERT_TRUE(Exists(removed_t2));
        EXPECT_EQ(304, Value(removed_t2).count());
      }
      current::time::SetNow(std::chrono::microseconds(305));
      fields.umany_to_umany.Erase(1, "x");
      {
        const auto t = fields.umany_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(305, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(306));
      fields.uone_to_umany.Add(Cell{42, "z", 42});
      {
        const auto t = fields.uone_to_umany.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(306, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(307));
      fields.uone_to_umany.EraseCol("z");
      {
        const auto t = fields.uone_to_umany.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(307, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(308));
      fields.uone_to_umany.Erase(2, "x");
      {
        const auto t = fields.uone_to_umany.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(308, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(309));
      fields.uone_to_uone.Add(Cell{2, "x", 2});
      {
        const auto t = fields.uone_to_uone.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(309, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(310));
      fields.uone_to_uone.Add(Cell{42, "z", 42});
      {
        const auto t = fields.uone_to_uone.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(310, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(311));
      fields.uone_to_uone.EraseRow(42);
      {
        const auto t = fields.uone_to_uone.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(311, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(312));
      fields.uone_to_uone.EraseCol("x");
      {
        const auto t = fields.uone_to_uone.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(312, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(313));
      fields.uone_to_uone.Erase(1, "y");
      {
        const auto t = fields.uone_to_uone.LastModified(1, "y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(313, Value(t).count());
      }
    }).Go();
    EXPECT_TRUE(WasCommitted(result));

    ASSERT_EQ(2u, persister.Size());
    const auto& transaction = TransactionByIndex(1u);
    EXPECT_EQ(300, transaction.meta.begin_us.count());
    EXPECT_EQ(313, transaction.meta.end_us.count());
    ASSERT_EQ(16u, transaction.mutations.size());
    ASSERT_TRUE(Exists<CellUnorderedManyToUnorderedManyUpdated>(transaction.mutations[0]));
    EXPECT_EQ(301, Value<CellUnorderedManyToUnorderedManyUpdated>(transaction.mutations[0]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[1]));
    EXPECT_EQ(302, Value<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[1]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[2]));
    EXPECT_EQ(302, Value<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[2]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[3]));
    EXPECT_EQ(303, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[3]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[4]));
    EXPECT_EQ(304, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[4]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[5]));
    EXPECT_EQ(304, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[5]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[6]));
    EXPECT_EQ(304, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[6]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedManyToUnorderedManyDeleted>(transaction.mutations[7]));
    EXPECT_EQ(305, Value<CellUnorderedManyToUnorderedManyDeleted>(transaction.mutations[7]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[8]));
    EXPECT_EQ(306, Value<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[8]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[9]));
    EXPECT_EQ(307, Value<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[9]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[10]));
    EXPECT_EQ(308, Value<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[10]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[11]));
    EXPECT_EQ(309, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[11]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[12]));
    EXPECT_EQ(310, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[12]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[13]));
    EXPECT_EQ(311, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[13]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[14]));
    EXPECT_EQ(312, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[14]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[15]));
    EXPECT_EQ(313, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[15]).us.count());
  }
}

TEST(TransactionalStorage, ReplicationViaHTTP) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
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
    const auto result = master_storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      current::time::SetNow(std::chrono::microseconds(101));
      fields.d.Add(Record{"one", 1});
      current::time::SetNow(std::chrono::microseconds(102));
      fields.d.Add(Record{"two", 2});
      fields.SetTransactionMetaField("user", "dima");
      current::time::SetNow(std::chrono::microseconds(103));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = master_storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      current::time::SetNow(std::chrono::microseconds(201));
      fields.d.Add(Record{"three", 3});
      current::time::SetNow(std::chrono::microseconds(202));
      fields.d.Erase("two");
      current::time::SetNow(std::chrono::microseconds(203));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  // Confirm empty transactions are not persisted.
  {
    current::time::SetNow(std::chrono::microseconds(301));
    master_storage.ReadOnlyTransaction([](ImmutableFields<Storage>) {}).Wait();
  }
  {
    current::time::SetNow(std::chrono::microseconds(302));
    master_storage.ReadWriteTransaction([](MutableFields<Storage>) {}).Wait();
  }

  // Confirm the non-empty transactions have been persisted, while the empty ones have been skipped.
  EXPECT_EQ(current::FileSystem::ReadFileAsString(golden_storage_file_name),
            current::FileSystem::ReadFileAsString(master_storage_file_name));

  // Create stream for replication.
  const std::string replicated_stream_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data2");
  const auto replicated_stream_file_remover = current::FileSystem::ScopedRmFile(replicated_stream_file_name);
  using transaction_t = typename Storage::transaction_t;
  using sherlock_t = current::sherlock::Stream<transaction_t, current::persistence::File>;
  sherlock_t replicated_stream(replicated_stream_file_name);

  struct StreamPublisherOwner {
    sherlock_t& stream_;
    std::unique_ptr<sherlock_t::publisher_t> publisher_;
    explicit StreamPublisherOwner(sherlock_t& stream) : stream_(stream) {}
    ~StreamPublisherOwner() { EXPECT_FALSE(static_cast<bool>(publisher_)); }
    void AcceptPublisher(std::unique_ptr<sherlock_t::publisher_t> publisher) {
      publisher_ = std::move(publisher);
    }
    void ReturnPublisherToStream() { stream_.AcquirePublisher(std::move(publisher_)); }
  };
  // Move the data authority of the stream to a `StreamPublisherOwner` instance.
  StreamPublisherOwner stream_publisher_owner(replicated_stream);
  replicated_stream.MovePublisherTo(stream_publisher_owner);

  // Create storage using following stream.
  Storage replicated_storage(replicated_stream);

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
    auto transaction = ParseJSON<Storage::transaction_t>(line.substr(tab_pos + 1));
    ASSERT_NO_THROW(stream_publisher_owner.publisher_->Publish(std::move(transaction), idx_ts.us));
    ++expected_index;
  }

  // Return data authority to the stream as we completed the replication process.
  stream_publisher_owner.ReturnPublisherToStream();

  // Check that persisted files are the same.
  EXPECT_EQ(current::FileSystem::ReadFileAsString(master_storage_file_name),
            current::FileSystem::ReadFileAsString(replicated_stream_file_name));

  // Wait until the we can see the mutation made in the last transaction.
  bool last_added_entry_exists;
  do {
    const auto result = replicated_storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
      return Exists(fields.d["three"]);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
    last_added_entry_exists = Value(result);
  } while (!last_added_entry_exists);

  // Test data consistency performing a transaction in the replicated storage.
  const auto result = replicated_storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
    EXPECT_EQ(2u, fields.d.Size());
    EXPECT_EQ(1, Value(fields.d["one"]).rhs);
    EXPECT_EQ(3, Value(fields.d["three"]).rhs);
    EXPECT_FALSE(Exists(fields.d["two"]));
  }).Go();
  EXPECT_TRUE(WasCommitted(result));
}

namespace transactional_storage_test {

template <typename SHERLOCK_ENTRY>
class StorageSherlockTestProcessorImpl {
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;

 public:
  StorageSherlockTestProcessorImpl(std::string& output) : output_(output) {}

  void SetAllowTerminate() { allow_terminate_ = true; }
  void SetAllowTerminateOnOnMoreEntriesOfRightType() {
    allow_terminate_on_no_more_entries_of_right_type_ = true;
  }

  EntryResponse operator()(const SHERLOCK_ENTRY& entry, idxts_t current, idxts_t last) const {
    output_ += JSON(current) + '\t' + JSON(entry) + '\n';
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
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"one", 1});
      current::time::SetNow(std::chrono::microseconds(101));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"two", 2});
      current::time::SetNow(std::chrono::microseconds(201));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string collected;
  StorageSherlockTestProcessor<Storage::transaction_t> processor(collected);
  storage.InternalExposeStream().Subscribe(processor);
  EXPECT_EQ(
      "{\"index\":0,\"us\":101}\t{\"meta\":{\"begin_us\":100,\"end_us\":101,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"us\":100,\"data\":{\"lhs\":\"one\",\"rhs\":1}},"
      "\"\":\"T9200018162904582576\"}]}\n"
      "{\"index\":1,\"us\":201}\t{\"meta\":{\"begin_us\":200,\"end_us\":201,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"us\":200,\"data\":{\"lhs\":\"two\",\"rhs\":2}},"
      "\"\":\"T9200018162904582576\"}]}\n",
      collected);
}

TEST(TransactionalStorage, GracefulShutdown) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  Storage storage;
  storage.GracefulShutdown();
  auto result = storage.ReadOnlyTransaction([](ImmutableFields<Storage>) {});
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
  using brief_t = SUPER;
  CURRENT_FIELD(details, Optional<std::string>);
  CURRENT_CONSTRUCTOR(SimpleLike)(const std::string& who = "", const std::string& what = "")
      : SUPER(who, what) {}
  CURRENT_CONSTRUCTOR(SimpleLike)(const std::string& who, const std::string& what, const std::string& details)
      : SUPER(who, what), details(details) {}
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, SimpleUser, SimpleUserPersisted);  // Ordered for list view.
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, SimplePost, SimplePostPersisted);
// TODO(dkorolev): Ordered `ManyToMany` too.
CURRENT_STORAGE_FIELD_ENTRY(UnorderedManyToUnorderedMany, SimpleLike, SimpleLikePersisted);

CURRENT_STORAGE(SimpleStorage) {
  CURRENT_STORAGE_FIELD(user, SimpleUserPersisted);
  CURRENT_STORAGE_FIELD(post, SimplePostPersisted);
  CURRENT_STORAGE_FIELD(like, SimpleLikePersisted);
};

}  // namespace transactional_storage_test

// RESTful API test.
TEST(TransactionalStorage, RESTfulAPITest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using Storage = SimpleStorage<JSONFilePersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  EXPECT_EQ(3u, Storage::FIELDS_COUNT);
  Storage storage(persistence_file_name);

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_transactional_storage_test_port);

  const std::string golden_user_schema_h =
      "// The `current.h` file is the one from `https://github.com/C5T/Current`.\n"
      "// Compile with `-std=c++11` or higher.\n"
      "\n"
      "#ifndef CURRENT_USERSPACE_C546BDBF8F3A5614\n"
      "#define CURRENT_USERSPACE_C546BDBF8F3A5614\n"
      "\n"
      "#include \"current.h\"\n"
      "\n"
      "// clang-format off\n"
      "\n"
      "namespace current_userspace_c546bdbf8f3a5614 {\n"
      "\n"
      "CURRENT_STRUCT(SimpleUser) {\n"
      "  CURRENT_FIELD(key, std::string);\n"
      "  CURRENT_FIELD(name, std::string);\n"
      "};\n"
      "using T9204302787243580577 = SimpleUser;\n"
      "\n"
      "}  // namespace current_userspace_c546bdbf8f3a5614\n"
      "\n"
      "CURRENT_NAMESPACE(USERSPACE_C546BDBF8F3A5614) {\n"
      "  CURRENT_NAMESPACE_TYPE(SimpleUser, current_userspace_c546bdbf8f3a5614::SimpleUser);\n"
      "};  // CURRENT_NAMESPACE(USERSPACE_C546BDBF8F3A5614)\n"
      "\n"
      "namespace current {\n"
      "namespace type_evolution {\n"
      "\n"
      "// Default evolution for struct `SimpleUser`.\n"
      "#ifndef DEFAULT_EVOLUTION_03EF01DB8AAC8284F79033E453C68C55958D143788D6A43EC1F8ED9B22050D10  // typename "
      "USERSPACE_C546BDBF8F3A5614::SimpleUser\n"
      "#define DEFAULT_EVOLUTION_03EF01DB8AAC8284F79033E453C68C55958D143788D6A43EC1F8ED9B22050D10  // typename "
      "USERSPACE_C546BDBF8F3A5614::SimpleUser\n"
      "template <typename FROM, typename EVOLUTOR>\n"
      "struct Evolve<FROM, typename USERSPACE_C546BDBF8F3A5614::SimpleUser, EVOLUTOR> {\n"
      "  template <typename INTO,\n"
      "            class CHECK = FROM,\n"
      "            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_C546BDBF8F3A5614, "
      "CHECK>::value>>\n"
      "  static void Go(const typename USERSPACE_C546BDBF8F3A5614::SimpleUser& from,\n"
      "                 typename INTO::SimpleUser& into) {\n"
      "      static_assert(::current::reflection::FieldCounter<typename INTO::SimpleUser>::value == 2,\n"
      "                    \"Custom evolutor required.\");\n"
      "      Evolve<FROM, decltype(from.key), EVOLUTOR>::template Go<INTO>(from.key, into.key);\n"
      "      Evolve<FROM, decltype(from.name), EVOLUTOR>::template Go<INTO>(from.name, into.name);\n"
      "  }\n"
      "};\n"
      "#endif\n"
      "\n"
      "}  // namespace current::type_evolution\n"
      "}  // namespace current\n"
      "\n"
      "#if 0  // Boilerplate evolutors.\n"
      "\n"
      "CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_C546BDBF8F3A5614, SimpleUser, {\n"
      "  CURRENT_NATURAL_EVOLVE(USERSPACE_C546BDBF8F3A5614, CustomDestinationNamespace, from.key, into.key);\n"
      "  CURRENT_NATURAL_EVOLVE(USERSPACE_C546BDBF8F3A5614, CustomDestinationNamespace, from.name, "
      "into.name);\n"
      "});\n"
      "\n"
      "#endif  // Boilerplate evolutors.\n"
      "\n"
      "// clang-format on\n"
      "\n"
      "#endif  // CURRENT_USERSPACE_C546BDBF8F3A5614\n";

  // clang-format off
  const std::string golden_user_schema_json =
  "["
    "{\"object\":\"SimpleUser\",\"contains\":["
      "{\"field\":\"key\",\"as\":{\"primitive\":{\"type\":\"std::string\",\"text\":\"String\"}}},"
      "{\"field\":\"name\",\"as\":{\"primitive\":{\"type\":\"std::string\",\"text\":\"String\"}}}"
    "]}"
  "]";
  // clang-format on

  // Run twice to make sure the `GET-POST-GET-DELETE` cycle is complete.
  for (size_t i = 0; i < 2; ++i) {
    // Register RESTful HTTP endpoints, in a scoped way.
    auto rest = RESTfulStorage<Storage>(
        storage, FLAGS_transactional_storage_test_port, "/api", "http://unittest.current.ai");
    // Confirm the schema is returned.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/schema/user")).code));
    EXPECT_EQ("SimpleUser", HTTP(GET(base_url + "/api/schema/user")).body);
    EXPECT_EQ(golden_user_schema_h, HTTP(GET(base_url + "/api/schema/user.h")).body);
    EXPECT_EQ(golden_user_schema_json, HTTP(GET(base_url + "/api/schema/user.json")).body);
    EXPECT_EQ("SimplePost", HTTP(GET(base_url + "/api/schema/post")).body);
    EXPECT_EQ("SimpleLike", HTTP(GET(base_url + "/api/schema/like")).body);
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/schema/user_alias")).code));

    rest.RegisterAlias("user", "user_alias");

    // Confirm the schema is returned as the alias too.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api/schema/user_alias")).code));
    EXPECT_EQ("SimpleUser", HTTP(GET(base_url + "/api/schema/user_alias")).body);
    EXPECT_EQ(golden_user_schema_h, HTTP(GET(base_url + "/api/schema/user_alias.h")).body);

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
    EXPECT_EQ("max-beer\ndima-beer\n", HTTP(GET(base_url + "/api/data/like")).body);

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
  current::time::ResetToZero();

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
  current::time::ResetToZero();

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
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = TestStorage<SherlockInMemoryStreamPersister>;

  static_assert(std::is_same<typename Storage::persister_t::sherlock_t,
                             current::sherlock::Stream<typename Storage::persister_t::transaction_t,
                                                       current::persistence::Memory>>::value,
                "");

  typename Storage::persister_t::sherlock_t stream;
  Storage storage(stream);

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.d.Add(Record{"own_stream", 42});
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string collected;
  StorageSherlockTestProcessor<Storage::transaction_t> processor(collected);
  storage.InternalExposeStream().Subscribe(processor);
  EXPECT_EQ(
      "{\"index\":0,\"us\":100}\t{\"meta\":{\"begin_us\":100,\"end_us\":100,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"us\":100,\"data\":{\"lhs\":\"own_stream\",\"rhs\":42}},\"\":"
      "\"T9200018162904582576\"}]}\n",
      collected);
}

namespace transactional_storage_test {

CURRENT_STRUCT(StreamEntryOutsideStorage) {
  CURRENT_FIELD(s, std::string);
  CURRENT_CONSTRUCTOR(StreamEntryOutsideStorage)(const std::string& s = "") : s(s) {}
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, UseExternallyProvidedSherlockStreamOfBroaderType) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using pre_storage_t = TestStorage<SherlockInMemoryStreamPersister>;
  using transaction_t = typename pre_storage_t::transaction_t;

  static_assert(std::is_same<transaction_t, typename pre_storage_t::persister_t::transaction_t>::value, "");

  using storage_t = TestStorage<SherlockInMemoryStreamPersister,
                                current::storage::transaction_policy::Synchronous,
                                Variant<transaction_t, StreamEntryOutsideStorage>>;

  static_assert(std::is_same<typename storage_t::persister_t::sherlock_t,
                             current::sherlock::Stream<Variant<transaction_t, StreamEntryOutsideStorage>,
                                                       current::persistence::Memory>>::value,
                "");

  typename storage_t::persister_t::sherlock_t stream;

  storage_t storage(stream);

  {
    // Add three records to the stream: first and third externally, second through the storage.
    { stream.Publish(StreamEntryOutsideStorage("one"), std::chrono::microseconds(1)); }

    {
      current::time::SetNow(std::chrono::microseconds(2));
      const auto result = storage.ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
    storage.InternalExposeStream().Subscribe<transaction_t>(processor);
    EXPECT_EQ(
        "{\"index\":1,\"us\":2}\t"
        "{\"meta\":{\"begin_us\":2,\"end_us\":2,\"fields\":{}},\"mutations\":["
        "{\"RecordDictionaryUpdated\":{\"us\":2,\"data\":{\"lhs\":\"two\",\"rhs\":2}},"
        "\"\":\"T9200018162904582576\"}]}\n",
        collected_transactions);
  }

  {
    // Subscribe to and collect non-transactions.
    std::string collected_non_transactions;
    StorageSherlockTestProcessor<StreamEntryOutsideStorage> processor(collected_non_transactions);
    processor.SetAllowTerminateOnOnMoreEntriesOfRightType();
    storage.InternalExposeStream().Subscribe<StreamEntryOutsideStorage>(processor);
    EXPECT_EQ("{\"index\":0,\"us\":1}\t{\"s\":\"one\"}\n{\"index\":2,\"us\":3}\t{\"s\":\"three\"}\n",
              collected_non_transactions);
  }

  {
    // Confirm replaying storage with a mixed-content stream does its job.
    storage_t replayed(stream);
    const auto result = replayed.ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
      EXPECT_EQ(1u, fields.d.Size());
      ASSERT_TRUE(Exists(fields.d["two"]));
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}

TEST(TransactionalStorage, FollowingStorageFlipsToMaster) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using Storage = SimpleStorage<SherlockStreamPersister>;
  using transaction_t = typename Storage::transaction_t;
  using sherlock_t = current::sherlock::Stream<transaction_t, current::persistence::File>;

  // Class performing stream-level replication.
  struct StreamReplicatorImpl {
    using EntryResponse = current::ss::EntryResponse;
    using TerminationResponse = current::ss::TerminationResponse;
    using publisher_t = typename sherlock_t::publisher_t;

    StreamReplicatorImpl(sherlock_t& stream) : stream_(stream) { stream.MovePublisherTo(*this); }
    ~StreamReplicatorImpl() { stream_.AcquirePublisher(std::move(publisher_)); }

    void AcceptPublisher(std::unique_ptr<publisher_t> publisher) { publisher_ = std::move(publisher); }

    EntryResponse operator()(const transaction_t& transaction, idxts_t current, idxts_t) {
      assert(publisher_);
      publisher_->Publish(transaction, current.us);
      return EntryResponse::More;
    }

    EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
    TerminationResponse Terminate() const { return TerminationResponse::Terminate; }

   private:
    sherlock_t& stream_;
    std::unique_ptr<publisher_t> publisher_;
  };
  using StreamReplicator = current::ss::StreamSubscriber<StreamReplicatorImpl, transaction_t>;

  const std::string master_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "master");
  const auto master_file_remover = current::FileSystem::ScopedRmFile(master_file_name);

  const std::string follower_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "follower");
  const auto follower_file_remover = current::FileSystem::ScopedRmFile(follower_file_name);

  sherlock_t follower_stream(follower_file_name);
  // Replicator acquires the stream's persister object in its constructor.
  auto replicator = std::make_unique<StreamReplicator>(follower_stream);

  // Underlying stream is created and owned by `master_storage`.
  Storage master_storage(master_file_name);
  // Follower storage is created atop a stream with external data authority.
  Storage follower_storage(follower_stream);

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_transactional_storage_test_port);

  // Start RESTful service atop follower storage.
  auto rest = RESTfulStorage<Storage>(
      follower_storage, FLAGS_transactional_storage_test_port, "/api", "http://unittest.current.ai");

  // Launch continuos replication process.
  {
    const auto replicator_scope =
        master_storage.InternalExposeStream().template Subscribe<transaction_t>(*replicator);

    // Confirm an empty collection is returned.
    {
      const auto result = HTTP(GET(base_url + "/api/data/user"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ("", result.body);
    }

    // Publish one record.
    current::time::SetNow(std::chrono::microseconds(100));
    {
      const auto result = master_storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
        fields.user.Add(SimpleUser("John", "JD"));
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }

    // Wait until the transaction performed above is replicated to the `follower_stream` and imported by
    // the `follower_storage`.
    {
      size_t user_size = 0u;
      do {
        const auto result = follower_storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
          return fields.user.Size();
        }).Go();
        EXPECT_TRUE(WasCommitted(result));
        user_size = Value(result);
      } while (user_size == 0u);
      EXPECT_EQ(1u, user_size);
    }

    // Check that the the following storage now has the same record as the master one.
    {
      const auto result = follower_storage.ReadOnlyTransaction([](ImmutableFields<Storage> fields) {
        ASSERT_TRUE(Exists(fields.user["John"]));
        EXPECT_EQ("JD", Value(fields.user["John"]).name);
      }).Go();
      EXPECT_TRUE(WasCommitted(result));
    }
    {
      const auto result = HTTP(GET(base_url + "/api/data/user/John"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ("JD", ParseJSON<SimpleUser>(result.body).name);
    }

    // Mutating access to the RESTful service of the `follower` is not allowed.
    {
      const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("max", "MZ")));
      EXPECT_EQ(405, static_cast<int>(post_response.code));
      const auto put_response = HTTP(PUT(base_url + "/api/data/user/John", SimpleUser("John", "DJ")));
      EXPECT_EQ(405, static_cast<int>(post_response.code));
      const auto delete_response = HTTP(DELETE(base_url + "/api/data/user/John"));
      EXPECT_EQ(405, static_cast<int>(post_response.code));
    }

    // Attempt to run read-write transaction in `Follower` mode throws an exception.
    EXPECT_THROW(follower_storage.ReadWriteTransaction([](MutableFields<Storage>) {}),
                 current::storage::ReadWriteTransactionInFollowerStorageException);
    EXPECT_THROW(follower_storage.ReadWriteTransaction([](MutableFields<Storage>) { return 42; }, [](int) {}),
                 current::storage::ReadWriteTransactionInFollowerStorageException);

    // At this moment the content of both persisted files must be identical.
    EXPECT_EQ(current::FileSystem::ReadFileAsString(master_file_name),
              current::FileSystem::ReadFileAsString(follower_file_name));

    // `FlipToMaster()` method on a storage with `Master` role throws an exception.
    EXPECT_THROW(master_storage.FlipToMaster(), current::storage::StorageIsAlreadyMasterException);
    // Publisher of the `follower_stream` is still in the `replicator`.
    EXPECT_THROW(follower_storage.FlipToMaster(),
                 current::storage::UnderlyingStreamHasExternalDataAuthorityException);

    // Stop the replication process by ending its scope.
  }

  // `StreamReplicator` returns publisher to the stream in its destructor.
  replicator = nullptr;
  // Switch to a `Master` role.
  ASSERT_NO_THROW(follower_storage.FlipToMaster());

  // Publish record and check that everything goes well.
  current::time::SetNow(std::chrono::microseconds(200));
  {
    const auto result = follower_storage.ReadWriteTransaction([](MutableFields<Storage> fields) {
      fields.user.Add(SimpleUser("max", "MZ"));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  current::time::SetNow(std::chrono::microseconds(300));
  {
    // Publish one more record using REST.
    const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("dima", "DK")));
    EXPECT_EQ(201, static_cast<int>(post_response.code));
    const auto user_key = post_response.body;

    // Ensure that all the records are in place.
    const auto result = follower_storage.ReadOnlyTransaction([user_key](ImmutableFields<Storage> fields) {
      EXPECT_EQ(3u, fields.user.Size());
      ASSERT_TRUE(Exists(fields.user["max"]));
      EXPECT_EQ("MZ", Value(fields.user["max"]).name);
      ASSERT_TRUE(Exists(fields.user[user_key]));
      EXPECT_EQ("DK", Value(fields.user[user_key]).name);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}
