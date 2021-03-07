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

#include "../test_helpers.cc"

#ifndef STORAGE_ONLY_RUN_RESTFUL_TESTS

#include "../docu/docu_2_code.cc"
#include "../docu/docu_3_code.cc"

TEST(TransactionalStorage, SmokeTest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamStreamPersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    EXPECT_EQ(13u, storage_t::FIELDS_COUNT);

    auto stream = storage_t::stream_t::CreateStream(persistence_file_name);
    current::Owned<storage_t> storage = storage_t::CreateMasterStorageAtopExistingStream(stream);

    EXPECT_TRUE(storage->IsMasterStorage());

    // Fill a `Dictionary` container.
    {
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
        EXPECT_FALSE(fields.d.Has("one"));
        fields.d.Add(Record{"one", 1});
        EXPECT_TRUE(fields.d.Has("one"));

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
        EXPECT_TRUE(fields.d.Has("three"));
        fields.d.Erase("three");
        EXPECT_FALSE(fields.d.Has("three"));
      }).Go();

      EXPECT_TRUE(WasCommitted(result));
    }

    // Fill a `ManyToMany` container.
    {
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
        EXPECT_TRUE(fields.umany_to_umany.Empty());
        EXPECT_EQ(0u, fields.umany_to_umany.Size());
        EXPECT_TRUE(fields.umany_to_umany.Rows().Empty());
        EXPECT_TRUE(fields.umany_to_umany.Cols().Empty());
        EXPECT_EQ(0u, fields.umany_to_umany.Rows().Size());
        EXPECT_EQ(0u, fields.umany_to_umany.Cols().Size());
        EXPECT_FALSE(fields.umany_to_umany.Has(1, "one"));
        EXPECT_FALSE(fields.umany_to_umany.Has(std::make_pair(1, "one")));
        fields.umany_to_umany.Add(Cell{1, "one", 1});
        EXPECT_TRUE(fields.umany_to_umany.Has(1, "one"));
        EXPECT_TRUE(fields.umany_to_umany.Has(std::make_pair(1, "one")));
        EXPECT_FALSE(fields.umany_to_umany.Has(101, "one"));
        EXPECT_FALSE(fields.umany_to_umany.Has(1, "some one"));
        EXPECT_FALSE(fields.umany_to_umany.Has(std::make_pair(101, "one")));
        EXPECT_FALSE(fields.umany_to_umany.Has(std::make_pair(1, "some one")));
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
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
        EXPECT_TRUE(fields.uone_to_umany.Empty());
        EXPECT_EQ(0u, fields.uone_to_umany.Size());
        EXPECT_TRUE(fields.uone_to_umany.Rows().Empty());
        EXPECT_TRUE(fields.uone_to_umany.Cols().Empty());
        EXPECT_EQ(0u, fields.uone_to_umany.Rows().Size());
        EXPECT_EQ(0u, fields.uone_to_umany.Cols().Size());

        EXPECT_FALSE(fields.uone_to_umany.Has(1, "one"));
        EXPECT_FALSE(fields.uone_to_umany.Has(std::make_pair(1, "one")));

        fields.uone_to_umany.Add(Cell{1, "one", 1});   // Adds {1,one=1 }
        fields.uone_to_umany.Add(Cell{2, "two", 5});   // Adds {2,two=5 }
        fields.uone_to_umany.Add(Cell{2, "two", 4});   // Adds {2,two=4 }, overwrites {2,two=5}
        fields.uone_to_umany.Add(Cell{1, "fiv", 5});   // Adds {1,fiv=5 }
        fields.uone_to_umany.Add(Cell{2, "fiv", 10});  // Adds {2,fiv=10}, removes {1,fiv=5}
        fields.uone_to_umany.Add(Cell{3, "six", 18});  // Adds {3,six=18}
        fields.uone_to_umany.Add(Cell{1, "six", 6});   // Adds {1,six=6 }, removes {3,six=18}

        EXPECT_TRUE(fields.uone_to_umany.Has(1, "one"));
        EXPECT_TRUE(fields.uone_to_umany.Has(std::make_pair(1, "one")));
        EXPECT_FALSE(fields.uone_to_umany.Has(1, "fiv"));  // Implicitly removed above.
        EXPECT_FALSE(fields.uone_to_umany.Has(3, "six"));  // Implicitly removed above.
        EXPECT_FALSE(fields.uone_to_umany.Has(std::make_pair(1, "fiv")));
        EXPECT_FALSE(fields.uone_to_umany.Has(std::make_pair(3, "six")));

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
      current::time::SetNow(std::chrono::microseconds(1000000));
      const auto result1 = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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

      current::time::SetNow(std::chrono::microseconds(2000000));
      const auto result2 = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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

      current::time::SetNow(std::chrono::microseconds(3000000));
      const auto result3 = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
      const auto result1 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
        EXPECT_FALSE(fields.umany_to_umany.Empty());
        EXPECT_FALSE(fields.omany_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto row : fields.umany_to_umany.Rows()) {
          for (const auto& element : row) {
            data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                            current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                            current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto row : fields.omany_to_omany.Rows()) {
          for (const auto& element : row) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 2,too=3 2,two=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        std::multiset<int32_t> rows;
        for (const auto row : fields.umany_to_omany.Rows()) {
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
        for (const auto row : fields.omany_to_umany.Rows()) {
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
      const auto result2 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
        EXPECT_FALSE(fields.umany_to_umany.Empty());
        EXPECT_FALSE(fields.omany_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto col : fields.umany_to_umany.Cols()) {
          for (const auto& element : col) {
            data_set.insert(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                            current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                            current::ToString(element.phew));
          }
        }
        EXPECT_EQ("one,1=1 too,2=3 two,2=2", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto col : fields.omany_to_omany.Cols()) {
          for (const auto& element : col) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetCol(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetRow(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("one,1=1 too,2=3 two,2=2", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        for (const auto col : fields.umany_to_omany.Cols()) {
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
        for (const auto col : fields.omany_to_umany.Cols()) {
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
      const auto result3 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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
      const auto result1 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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
      const auto result2 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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
      const auto result1 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
        EXPECT_FALSE(fields.uone_to_umany.Empty());
        EXPECT_FALSE(fields.oone_to_omany.Empty());
        std::multiset<std::string> data_set;
        for (const auto row : fields.uone_to_umany.Rows()) {
          for (const auto& element : row) {
            data_set.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                            current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                            current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 1,six=6 2,fiv=10 2,two=4", current::strings::Join(data_set, ' '));
        // Use vector instead of set and expect the same result, because the data is already sorted.
        std::vector<std::string> data_vec;
        for (const auto row : fields.oone_to_omany.Rows()) {
          for (const auto& element : row) {
            data_vec.push_back(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                               current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                               current::ToString(element.phew));
          }
        }
        EXPECT_EQ("1,one=1 1,six=6 2,fiv=10 2,two=4", current::strings::Join(data_vec, ' '));
        data_vec.clear();
        std::multiset<int32_t> rows;
        for (const auto row : fields.uone_to_omany.Rows()) {
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
        for (const auto row : fields.oone_to_umany.Rows()) {
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
      const auto result2 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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
      const auto result3 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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
      const auto result1 = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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

      const auto result2 = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
        for (const auto row : fields.umany_to_umany.Rows()) {
          for (const auto& element : row) {
            data1.insert(current::ToString(current::storage::sfinae::GetRow(element)) + ',' +
                         current::ToString(current::storage::sfinae::GetCol(element)) + '=' +
                         current::ToString(element.phew));
          }
        }
        for (const auto col : fields.umany_to_umany.Cols()) {
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
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
      const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
        for (const auto row : fields.uone_to_umany.Rows()) {
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
      const auto f = [](ImmutableFields<storage_t>) { return 42; };

      const auto result = storage->ReadOnlyTransaction(f).Go();
      EXPECT_TRUE(WasCommitted(result));
      EXPECT_TRUE(Exists(result));
      EXPECT_EQ(42, Value(result));
    }

    EXPECT_EQ(static_cast<uint64_t>(7), stream->Data()->Size());
  }

  // Replay the entire storage from file.
  {
    current::Owned<storage_t> replayed = storage_t::CreateFollowingStorage(persistence_file_name);

    while (replayed->LastAppliedTimestamp() < std::chrono::microseconds(3000000)) {
      std::this_thread::yield();
    }

    const auto result = replayed->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
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

TEST(TransactionalStorage, ExposedFieldNames) {
  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  auto storage = storage_t::CreateMasterStorage();
  const auto result = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
    EXPECT_EQ("d", fields.d.FieldName());
    EXPECT_EQ("umany_to_umany", fields.umany_to_umany.FieldName());
    EXPECT_EQ("omany_to_omany", fields.omany_to_omany.FieldName());
    EXPECT_EQ("umany_to_omany", fields.umany_to_omany.FieldName());
    EXPECT_EQ("omany_to_umany", fields.omany_to_umany.FieldName());
    EXPECT_EQ("uone_to_uone", fields.uone_to_uone.FieldName());
    EXPECT_EQ("oone_to_oone", fields.oone_to_oone.FieldName());
    EXPECT_EQ("uone_to_oone", fields.uone_to_oone.FieldName());
    EXPECT_EQ("oone_to_uone", fields.oone_to_uone.FieldName());
    EXPECT_EQ("uone_to_umany", fields.uone_to_umany.FieldName());
    EXPECT_EQ("oone_to_omany", fields.oone_to_omany.FieldName());
    EXPECT_EQ("uone_to_omany", fields.uone_to_omany.FieldName());
    EXPECT_EQ("oone_to_umany", fields.oone_to_umany.FieldName());
  }).Go();
  ASSERT_TRUE(WasCommitted(result));
}

TEST(TransactionalStorage, FieldAccessors) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  EXPECT_EQ(13u, storage_t::FIELDS_COUNT);
  auto storage = storage_t::CreateMasterStorage();
  EXPECT_EQ("d", (*storage)(::current::storage::FieldNameByIndex<0>()));
  EXPECT_EQ("umany_to_umany", (*storage)(::current::storage::FieldNameByIndex<1>()));
  EXPECT_EQ("omany_to_omany", (*storage)(::current::storage::FieldNameByIndex<2>()));
  EXPECT_EQ("umany_to_omany", (*storage)(::current::storage::FieldNameByIndex<3>()));
  EXPECT_EQ("omany_to_umany", (*storage)(::current::storage::FieldNameByIndex<4>()));
  EXPECT_EQ("uone_to_uone", (*storage)(::current::storage::FieldNameByIndex<5>()));
  EXPECT_EQ("oone_to_oone", (*storage)(::current::storage::FieldNameByIndex<6>()));
  EXPECT_EQ("uone_to_oone", (*storage)(::current::storage::FieldNameByIndex<7>()));
  EXPECT_EQ("oone_to_uone", (*storage)(::current::storage::FieldNameByIndex<8>()));
  EXPECT_EQ("uone_to_umany", (*storage)(::current::storage::FieldNameByIndex<9>()));
  EXPECT_EQ("oone_to_omany", (*storage)(::current::storage::FieldNameByIndex<10>()));
  EXPECT_EQ("uone_to_omany", (*storage)(::current::storage::FieldNameByIndex<11>()));
  EXPECT_EQ("oone_to_umany", (*storage)(::current::storage::FieldNameByIndex<12>()));

  {
    std::string s;
    (*storage)(::current::storage::FieldNameAndTypeByIndex<0>(), CurrentStorageTestMagicTypesExtractor(s));
    EXPECT_EQ("d, OrderedDictionary, Record", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<1, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("umany_to_umany, UnorderedManyToUnorderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<2, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("omany_to_omany, OrderedManyToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<3, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("umany_to_omany, UnorderedManyToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<4, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("omany_to_umany, OrderedManyToUnorderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<5, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_uone, UnorderedOneToUnorderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<6, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_oone, OrderedOneToOrderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<7, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_oone, UnorderedOneToOrderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<8, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_uone, OrderedOneToUnorderedOne, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<9, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_umany, UnorderedOneToUnorderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<10, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_omany, OrderedOneToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<11, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("uone_to_omany, UnorderedOneToOrderedMany, Cell", s);
  }

  {
    std::string s;
    EXPECT_EQ(42,
              (*storage)(::current::storage::FieldNameAndTypeByIndexAndReturn<12, int>(),
                         CurrentStorageTestMagicTypesExtractor(s)));
    EXPECT_EQ("oone_to_umany, OrderedOneToUnorderedMany, Cell", s);
  }
}

TEST(TransactionalStorage, Exceptions) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  current::Owned<storage_t> storage = storage_t::CreateMasterStorage();

  bool should_throw;
  const auto f_void = [&should_throw](ImmutableFields<storage_t>) {
    if (should_throw) {
      CURRENT_STORAGE_THROW_ROLLBACK();
    }
  };

  const auto f_void_custom_exception = [&should_throw](ImmutableFields<storage_t>) {
    if (should_throw) {
      throw std::logic_error("wtf");
    }
  };

  bool throw_with_value;
  const auto f_int = [&should_throw, &throw_with_value](ImmutableFields<storage_t>) {
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

  const auto f_int_custom_exception = [&should_throw](ImmutableFields<storage_t>) {
    if (should_throw) {
      throw 100500;
    } else {
      return 42;  // LCOV_EXCL_LINE
    }
  };

  // `void` lambda successfully returned.
  {
    should_throw = false;
    const auto result = storage->ReadOnlyTransaction(f_void).Go();
    EXPECT_TRUE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
  }

  // `void` lambda with rollback requested by user.
  {
    should_throw = true;
    const auto result = storage->ReadOnlyTransaction(f_void).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
  }

  // `void` lambda with exception.
  {
    should_throw = true;
    auto result = storage->ReadOnlyTransaction(f_void_custom_exception);
    result.Wait();
    // We can ignore the exception until we try to get the result.
    EXPECT_THROW(result.Go(), std::logic_error);
  }

  // `int` lambda successfully returned.
  {
    should_throw = false;
    const auto result = storage->ReadOnlyTransaction(f_int).Go();
    EXPECT_TRUE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
    EXPECT_EQ(42, Value(result));
  }

  // `int` lambda with rollback not returning the value requested by user.
  {
    should_throw = true;
    throw_with_value = false;
    const auto result = storage->ReadOnlyTransaction(f_int).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_FALSE(Exists(result));
  }

  // `int` lambda with rollback returning the value requested by user.
  {
    should_throw = true;
    throw_with_value = true;
    const auto result = storage->ReadOnlyTransaction(f_int).Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_TRUE(Exists(result));
    EXPECT_EQ(-1, Value(result));
  }

  // `int` lambda with exception.
  {
    should_throw = true;
    bool was_thrown = false;
    try {
      storage->ReadOnlyTransaction(f_int_custom_exception).Go();
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
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  current::Owned<storage_t> storage = storage_t::CreateMasterStorage();
  const auto& data = storage->UnderlyingStream()->Data();
  const auto TransactionByIndex =
      [&data](uint64_t index) -> const storage_t::transaction_t& { return (*data->Iterate(static_cast<size_t>(index)).begin()).entry; };

  // Try to delete nonexistent entry, setting `who` transaction meta field.
  // No state should be changed as a result of executing the transaction.
  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      fields.d.Erase("nonexistent");
      fields.SetTransactionMetaField("who", "anyone");
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  // The same for two-step transaction with user-initiated rollback.
  {
    current::time::SetNow(std::chrono::microseconds(200));
    bool second_step_executed = false;
    const auto result = storage->ReadWriteTransaction(
                                     [](MutableFields<storage_t> fields) -> int {
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
    auto future = storage->ReadWriteTransaction(
        [](MutableFields<storage_t> fields) -> int {
          fields.d.Add(Record{"nonexistent2", 0});
          fields.SetTransactionMetaField("when", "now");
          CURRENT_THROW(current::Exception());
          return 0;
        },
        // LCOV_EXCL_START
        [&second_step_executed](int v) {
          EXPECT_EQ(0, v);
          second_step_executed = true;
        }
        // LCOV_EXCL_STOP
        );
    EXPECT_THROW(future.Go(), current::Exception);
    EXPECT_FALSE(second_step_executed);
  }

  // Add one entry, setting another meta field - `why`.
  {
    current::time::SetNow(std::chrono::microseconds(1000));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      fields.d.Add(Record{"", 42});
      fields.SetTransactionMetaField("why", "because");
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  // Check that only the second transaction has been persisted with the sole meta field `why`.
  {
    ASSERT_EQ(1u, data->Size());
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
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  current::Owned<storage_t> storage = storage_t::CreateMasterStorage();
  const auto& data = storage->UnderlyingStream()->Data();
  const auto TransactionByIndex =
      [&data](uint64_t index) -> const storage_t::transaction_t& { return (*(data->Iterate(index).begin())).entry; };

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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

    ASSERT_EQ(1u, data->Size());
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
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      fields.d.Add(Record{"x", 100});
      fields.d.Erase("y");
      fields.d.Add(Record{"z", 3});
      CURRENT_STORAGE_THROW_ROLLBACK();
    }).Go();
    EXPECT_FALSE(WasCommitted(result));
  }

  {
    current::time::SetNow(std::chrono::microseconds(300));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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

    ASSERT_EQ(2u, data->Size());
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
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  current::Owned<storage_t> storage = storage_t::CreateMasterStorage();
  const auto& data = storage->UnderlyingStream()->Data();
  const auto TransactionByIndex =
      [&data](uint64_t index) -> const storage_t::transaction_t& { return (*(data->Iterate(index).begin())).entry; };

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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

    ASSERT_EQ(1u, data->Size());
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
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
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
      current::time::SetNow(std::chrono::microseconds(302), std::chrono::microseconds(303));
      fields.uone_to_umany.Add(Cell{2, "x", 100});
      {
        const auto removed_t = fields.uone_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(removed_t));
        EXPECT_EQ(302, Value(removed_t).count());
        const auto t = fields.uone_to_umany.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(303, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(304));
      fields.uone_to_uone.Add(Cell{2, "y", 100});
      {
        const auto t = fields.uone_to_uone.LastModified(2, "y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(304, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(305), std::chrono::microseconds(307));
      fields.uone_to_uone.Add(Cell{1, "y", 42});
      {
        const auto removed_t1 = fields.uone_to_uone.LastModified(1, "x");
        ASSERT_TRUE(Exists(removed_t1));
        EXPECT_EQ(305, Value(removed_t1).count());
        const auto removed_t2 = fields.uone_to_uone.LastModified(2, "y");
        ASSERT_TRUE(Exists(removed_t2));
        EXPECT_EQ(306, Value(removed_t2).count());
        const auto t = fields.uone_to_uone.LastModified(1, "y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(307, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(308));
      fields.umany_to_umany.Erase(1, "x");
      {
        const auto t = fields.umany_to_umany.LastModified(1, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(308, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(309));
      fields.uone_to_umany.Add(Cell{42, "z", 42});
      {
        const auto t = fields.uone_to_umany.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(309, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(310));
      fields.uone_to_umany.EraseCol("z");
      {
        const auto t = fields.uone_to_umany.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(310, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(311));
      fields.uone_to_umany.Erase(2, "x");
      {
        const auto t = fields.uone_to_umany.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(311, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(312));
      fields.uone_to_uone.Add(Cell{2, "x", 2});
      {
        const auto t = fields.uone_to_uone.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(312, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(313));
      fields.uone_to_uone.Add(Cell{42, "z", 42});
      {
        const auto t = fields.uone_to_uone.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(313, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(314));
      fields.uone_to_uone.EraseRow(42);
      {
        const auto t = fields.uone_to_uone.LastModified(42, "z");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(314, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(315));
      fields.uone_to_uone.EraseCol("x");
      {
        const auto t = fields.uone_to_uone.LastModified(2, "x");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(315, Value(t).count());
      }
      current::time::SetNow(std::chrono::microseconds(316));
      fields.uone_to_uone.Erase(1, "y");
      {
        const auto t = fields.uone_to_uone.LastModified(1, "y");
        ASSERT_TRUE(Exists(t));
        EXPECT_EQ(316, Value(t).count());
      }
    }).Go();
    EXPECT_TRUE(WasCommitted(result));

    ASSERT_EQ(2u, data->Size());
    const auto& transaction = TransactionByIndex(1u);
    EXPECT_EQ(300, transaction.meta.begin_us.count());
    EXPECT_EQ(316, transaction.meta.end_us.count());
    ASSERT_EQ(16u, transaction.mutations.size());
    ASSERT_TRUE(Exists<CellUnorderedManyToUnorderedManyUpdated>(transaction.mutations[0]));
    EXPECT_EQ(301, Value<CellUnorderedManyToUnorderedManyUpdated>(transaction.mutations[0]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[1]));
    EXPECT_EQ(302, Value<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[1]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[2]));
    EXPECT_EQ(303, Value<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[2]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[3]));
    EXPECT_EQ(304, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[3]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[4]));
    EXPECT_EQ(305, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[4]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[5]));
    EXPECT_EQ(306, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[5]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[6]));
    EXPECT_EQ(307, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[6]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedManyToUnorderedManyDeleted>(transaction.mutations[7]));
    EXPECT_EQ(308, Value<CellUnorderedManyToUnorderedManyDeleted>(transaction.mutations[7]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[8]));
    EXPECT_EQ(309, Value<CellUnorderedOneToUnorderedManyUpdated>(transaction.mutations[8]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[9]));
    EXPECT_EQ(310, Value<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[9]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[10]));
    EXPECT_EQ(311, Value<CellUnorderedOneToUnorderedManyDeleted>(transaction.mutations[10]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[11]));
    EXPECT_EQ(312, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[11]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[12]));
    EXPECT_EQ(313, Value<CellUnorderedOneToUnorderedOneUpdated>(transaction.mutations[12]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[13]));
    EXPECT_EQ(314, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[13]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[14]));
    EXPECT_EQ(315, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[14]).us.count());
    ASSERT_TRUE(Exists<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[15]));
    EXPECT_EQ(316, Value<CellUnorderedOneToUnorderedOneDeleted>(transaction.mutations[15]).us.count());
  }
}

TEST(TransactionalStorage, WaitUntilLocalLogIsReplayed) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamStreamPersister>;
  using stream_t = typename storage_t::stream_t;

  const std::string storage_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "storage_data");
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(storage_file_name);
  // Write mutation log.
  {
    auto stream = storage_t::stream_t::CreateStream(storage_file_name);
    current::Owned<storage_t> master_storage = storage_t::CreateMasterStorageAtopExistingStream(stream);
    master_storage->ReadWriteTransaction([](MutableFields<storage_t> fields) { fields.d.Add(Record{"one", 1}); }).Go();
    master_storage->ReadWriteTransaction([](MutableFields<storage_t> fields) { fields.d.Add(Record{"two", 2}); }).Go();
    master_storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      fields.d.Add(Record{"three", 3});
    }).Go();
  }

  // Test following storage.
  {
    // Create the stream using previously written log.
    auto owned_stream(stream_t::CreateStream(storage_file_name));
    EXPECT_TRUE(owned_stream->IsMasterStream());

    // Move the data authority away from the stream.
    current::Borrowed<stream_t::publisher_t> stream_publisher_owner = owned_stream->BecomeFollowingStream();
    EXPECT_FALSE(owned_stream->IsMasterStream());

    // Spawn the following storage.
    current::Owned<storage_t> storage = storage_t::CreateFollowingStorageAtopExistingStream(owned_stream);
    EXPECT_FALSE(storage->IsMasterStorage());
    EXPECT_EQ(3u, storage->UnderlyingStream()->Data()->Size());

    // Wait until the whole log is replayed.
    bool done = false;
    while (!done) {
      done = Value(storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields)
                                                    -> bool { return Exists(fields.d["three"]); }).Go());
    }

    // Check the data.
    const auto result = storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
      EXPECT_TRUE(Exists(fields.d["one"]));
      EXPECT_EQ(1, Value(fields.d["one"]).rhs);
      EXPECT_TRUE(Exists(fields.d["two"]));
      EXPECT_EQ(2, Value(fields.d["two"]).rhs);
      EXPECT_TRUE(Exists(fields.d["three"]));
      EXPECT_EQ(3, Value(fields.d["three"]).rhs);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));

    // TODO(dkorolev): Test for no write transaction possible?

    // owned_stream->BecomeMasterStream();  // REQUIRED FOR NO DEADLOCK. -- D.K.
  }
}

TEST(TransactionalStorage, WorkWithUnderlyingStream) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  auto storage = storage_t::CreateMasterStorage();

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      fields.d.Add(Record{"one", 1});
      current::time::SetNow(std::chrono::microseconds(101));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      fields.d.Add(Record{"two", 2});
      current::time::SetNow(std::chrono::microseconds(201));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string collected;
  StorageStreamTestProcessor<storage_t::transaction_t> processor(collected);
  storage->Subscribe(processor);
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
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  auto storage = storage_t::CreateMasterStorage();
  storage->GracefulShutdown();
  auto result = storage->ReadOnlyTransaction([](ImmutableFields<storage_t>) {});
  ASSERT_THROW(result.Go(), current::storage::StorageInGracefulShutdownException);
}

#endif  // STORAGE_ONLY_RUN_RESTFUL_TESTS
