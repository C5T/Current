/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../../yoda.h"
#include "../../test_types.h"

#include "../../../../Bricks/dflags/dflags.h"
#include "../../../../Bricks/3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(yoda_matrix_test_port, 8993, "");

using bricks::strings::Printf;

TEST(YodaMatrixEntry, Smoke) {
  typedef yoda::API<yoda::MatrixEntry<MatrixCell>> TestAPI;
  TestAPI api("YodaMatrixEntrySmokeTest");

  api.AsyncAdd(MatrixCell(5, "x", -1)).Wait();
  EXPECT_EQ(-1, api.AsyncGet(5, "x").Go().value);
  EXPECT_EQ(-1, api.Get(5, "x").value);

  // Callback version.
  struct CallbackTest {
    explicit CallbackTest(const size_t row,
                          const std::string& col,
                          const int value,
                          const bool expect_success = true)
        : row(row), col(col), value(value), expect_success(expect_success) {}

    void found(const MatrixCell& entry) const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_TRUE(expect_success);
      EXPECT_EQ(row, entry.row);
      EXPECT_EQ(col, entry.col);
      EXPECT_EQ(value, entry.value);
    }

    void not_found(const size_t row, const std::string& col) const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_FALSE(expect_success);
      EXPECT_EQ(this->row, row);
      EXPECT_EQ(this->col, col);
    }

    void added() const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_TRUE(expect_success);
    }

    void already_exists() const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_FALSE(expect_success);
    }

    const size_t row;
    const std::string col;
    const int value;
    const bool expect_success;
    mutable bool called = false;
  };

  const CallbackTest cbt1(5, "x", -1);
  api.AsyncGet(typename yoda::MatrixEntry<MatrixCell>::T_ROW(5),
               typename yoda::MatrixEntry<MatrixCell>::T_COL("x"),
               std::bind(&CallbackTest::found, &cbt1, std::placeholders::_1),
               std::bind(&CallbackTest::not_found, &cbt1, std::placeholders::_1, std::placeholders::_2));
  while (!cbt1.called) {
    ;  // Spin lock.
  }

  ASSERT_THROW(api.AsyncGet(5, "y").Go(), typename yoda::MatrixEntry<MatrixCell>::T_CELL_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.AsyncGet(5, "y").Go(), yoda::CellNotFoundCoverException);
  ASSERT_THROW(api.Get(1, "x"), typename yoda::MatrixEntry<MatrixCell>::T_CELL_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.Get(1, "x"), yoda::CellNotFoundCoverException);
  const CallbackTest cbt2(123, "no_entry", 0, false);
  api.AsyncGet(123,
               "no_entry",
               std::bind(&CallbackTest::found, &cbt2, std::placeholders::_1),
               std::bind(&CallbackTest::not_found, &cbt2, std::placeholders::_1, std::placeholders::_2));
  while (!cbt2.called) {
    ;  // Spin lock.
  }

  // Add three more key-value pairs, this time via the API.
  api.AsyncAdd(MatrixCell(5, "y", 15)).Wait();
  api.Add(MatrixCell(1, "x", -9));
  const CallbackTest cbt3(42, "the_answer", 1);
  api.AsyncAdd(typename yoda::MatrixEntry<MatrixCell>::T_ENTRY(42, "the_answer", 1),
               std::bind(&CallbackTest::added, &cbt3),
               std::bind(&CallbackTest::already_exists, &cbt3));
  while (!cbt3.called) {
    ;  // Spin lock.
  }

  EXPECT_EQ(15, api.Get(5, "y").value);
  EXPECT_EQ(-9, api.Get(1, "x").value);
  EXPECT_EQ(1, api.Get(42, "the_answer").value);

  // Check that default policy doesn't allow overwriting on Add().
  ASSERT_THROW(api.AsyncAdd(MatrixCell(5, "y", 8)).Go(),
               typename yoda::MatrixEntry<MatrixCell>::T_CELL_ALREADY_EXISTS_EXCEPTION);
  ASSERT_THROW(api.AsyncAdd(MatrixCell(5, "y", 100)).Go(), yoda::CellAlreadyExistsCoverException);
  ASSERT_THROW(api.Add(MatrixCell(1, "x", 2)),
               typename yoda::MatrixEntry<MatrixCell>::T_CELL_ALREADY_EXISTS_EXCEPTION);
  ASSERT_THROW(api.Add(MatrixCell(1, "x", 2)), yoda::CellAlreadyExistsCoverException);
  const CallbackTest cbt4(42, "the_answer", 0, false);
  api.AsyncAdd(typename yoda::MatrixEntry<MatrixCell>::T_ENTRY(42, "the_answer", 0),
               std::bind(&CallbackTest::added, &cbt4),
               std::bind(&CallbackTest::already_exists, &cbt4));
  while (!cbt4.called) {
    ;  // Spin lock.
  }

  // Test `Call`-based access.
  api.Call([](TestAPI::T_CONTAINER_WRAPPER cw) {
             const auto cells = yoda::MatrixEntry<MatrixCell>::Accessor(cw);

             EXPECT_TRUE(cells.Exists(5, "x"));
             EXPECT_FALSE(cells.Exists(5, "q"));

             // `Get()` syntax.
             EXPECT_EQ(-1, static_cast<const MatrixCell&>(cells.Get(5, "x")).value);
             EXPECT_FALSE(cells.Get(5, "q"));

             auto mutable_cells = yoda::MatrixEntry<MatrixCell>::Mutator(cw);
             mutable_cells.Add(MatrixCell(100, "z", 43));
             EXPECT_EQ(43, static_cast<const MatrixCell&>(cells.Get(100, "z")).value);
             EXPECT_EQ(43, static_cast<const MatrixCell&>(mutable_cells.Get(100, "z")).value);
           }).Wait();

  // Test HTTP subscription.
  HTTP(FLAGS_yoda_matrix_test_port).ResetAllHandlers();
  api.ExposeViaHTTP(FLAGS_yoda_matrix_test_port, "/data");
  const std::string Z = "";  // For `clang-format`-indentation purposes.
  EXPECT_EQ(Z + JSON(WithBaseType<Padawan>(MatrixCell(5, "x", -1)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(5, "y", 15)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(1, "x", -9)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(42, "the_answer", 1)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(MatrixCell(100, "z", 43)), "entry") + '\n',
            HTTP(GET(Printf("http://localhost:%d/data?cap=5", FLAGS_yoda_matrix_test_port))).body);
}
