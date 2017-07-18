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

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

#include "gen.h"

#include "../bricks/dflags/dflags.h"

DEFINE_size_t(rows, 10u, "Number of rows.");
DEFINE_size_t(cols, 10u, "Number of cols.");
DEFINE_double(scale, 10.0, "Exponential distribution parameter.");
DEFINE_bool(nulls, false, "Inject '\0'-s into strings as well.");
DEFINE_size_t(random_seed, 42, "Random seed.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  std::unordered_map<size_t, std::string> strings;
  CreateTSV([&strings](const std::vector<size_t>& row) {
    for (size_t i = 0; i < row.size(); ++i) {
      std::string& s = strings[row[i]];
      if (s.empty()) {
        do {
          s += 'a' + rand() % 26;
          if (FLAGS_nulls) {
            do {
              s += '\0';
            } while ((rand() & 31) == 31);
          }
        } while (rand() & 7);
      }
      std::cout << s << ((i + 1) == row.size() ? '\n' : '\t');
    }
  }, FLAGS_rows, FLAGS_cols, FLAGS_scale, FLAGS_random_seed);
}
