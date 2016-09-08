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

// TODO(batman): Test all exceptions.
// TODO(batman): Test '\0'-s within input strings.

#include "compacttsv.h"
#include "gen.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/strings/util.h"
#include "../Bricks/dflags/dflags.h"
#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_size_t(rows, 10u, "Number of rows.");
DEFINE_size_t(cols, 4u, "Number of cols.");
DEFINE_double(scale, 5.0, "Exponential distribution parameter.");
DEFINE_size_t(random_seed, 42, "Random seed.");
DEFINE_bool(benchmark, false, "Set to 'true' to measure how long does unpacking take.");

TEST(CompactTSV, Smoke) {
  const bool run_test =
      (std::make_tuple(FLAGS_rows, FLAGS_cols, FLAGS_scale, FLAGS_random_seed) == std::make_tuple(10u, 4u, 5.0, 42));

  std::string golden;
  std::ostringstream os;

  const auto t_a_begin = current::time::Now();
  CreateTSV([&os](const std::vector<size_t> &row) {
    for (size_t i = 0; i < row.size(); ++i) {
      os << std::setw(2) << row[i] << ((i + 1) == row.size() ? '\n' : ' ');
    }
  }, FLAGS_rows, FLAGS_cols, FLAGS_scale, FLAGS_random_seed);
  const auto t_a_end = current::time::Now();

  if (run_test) {
    golden =
        " 7  1  7  4\n"
        " 2  0  3  2\n"
        " 0  5  0  6\n"
        "13  0 24  4\n"
        " 4  0  0  3\n"
        " 2  0 18  1\n"
        " 0  4  2 20\n"
        " 3  9  5  2\n"
        " 0 14  4  2\n"
        " 0  1  1  5\n";
    EXPECT_EQ(golden, os.str());
  } else {
    // Test TSV -> packed -> TSV for custom-sized inputs too, if specified externally.
    golden = os.str();  // LCOV_EXCL_LINE
  }

  CompactTSV fast;
  const auto t_b_begin = current::time::Now();
  CreateTSV([&fast](const std::vector<size_t> &row) {
    std::vector<std::string> row_of_strings(row.size());
    for (size_t i = 0; i < row.size(); ++i) {
      row_of_strings[i] = current::ToString(row[i]);
    }
    fast(row_of_strings);
  }, FLAGS_rows, FLAGS_cols, FLAGS_scale, FLAGS_random_seed);
  const auto t_b_end = current::time::Now();
  fast.Finalize();

  std::ostringstream os2;
  const auto t_c_begin = current::time::Now();
  EXPECT_EQ(FLAGS_rows,
            CompactTSV::Unpack([&os2](const std::vector<std::string> &row) {
              for (size_t i = 0; i < row.size(); ++i) {
                os2 << std::setw(2) << row[i] << ((i + 1) == row.size() ? '\n' : ' ');
              }
            }, fast.GetPackedString()));
  EXPECT_EQ(golden, os2.str());
  const auto t_c_end = current::time::Now();

  const auto t_d_begin = current::time::Now();
  EXPECT_EQ(FLAGS_rows, CompactTSV::Unpack([](const std::vector<std::string> &) {}, fast.GetPackedString()));
  const auto t_d_end = current::time::Now();

  const auto t_e_begin = current::time::Now();
  EXPECT_EQ(FLAGS_rows,
            CompactTSV::Unpack([](const std::vector<std::pair<const char *, size_t>> &) {}, fast.GetPackedString()));
  const auto t_e_end = current::time::Now();

  const auto t_f_begin = current::time::Now();
  EXPECT_EQ(FLAGS_rows,
            CompactTSV::Unpack([](const std::vector<current::strings::UniqueChunk> &) {}, fast.GetPackedString()));
  const auto t_f_end = current::time::Now();

  if (FLAGS_benchmark) {
    // LCOV_EXCL_START
    const size_t golden_size = golden.length();
    const size_t packed_size = fast.GetPackedString().length();
    std::cerr << "Original TSV size:\t" << golden_size << "b, or\t" << golden_size / (1024u * 1024u) << "MB.\n";
    std::cerr << "Packed   TSV size:\t" << packed_size << "b, or\t" << packed_size / (1024u * 1024u) << "MB.\n";
    const double K = 1e-3;  // microseconds -> milliseconds.
    std::cerr << "Generate:                                   " << K *(t_a_end - t_a_begin).count() << "ms.\n";
    std::cerr << "Pack:                                       " << K *(t_b_end - t_b_begin).count() << "ms.\n";
    std::cerr << "Unpack into std::ostringstream:             " << K *(t_c_end - t_c_begin).count() << "ms.\n";
    std::cerr << "Unpack into std::string-s:                  " << K *(t_d_end - t_d_begin).count() << "ms.\n";
    std::cerr << "Unpack into std::pair<const char*, size_t>: " << K *(t_e_end - t_e_begin).count() << "ms.\n";
    std::cerr << "Unpack into UniqueChunk-s:                  " << K *(t_f_end - t_f_begin).count() << "ms.\n";
    // LCOV_EXCL_STOP
  }
}
