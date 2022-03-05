// To run: g++ -std=c++11 -O3 step4_rides_by_months_still_unoptimized.cc && time ./a.out | tee >(md5sum)
//
// Count rides by month, print the counters in the lexicographically sorted order of keys.
// The "canonical C++" implementation splitting into `Chunk`-s, not `std::string`-s.

#include <iostream>
#include <unordered_map>

#include "../../../bricks/time/chrono.h"
#include "../../../bricks/file/file.h"
#include "../../../bricks/strings/strings.h"

int main() {
  // Go through the file and compute the counters.
  std::unordered_map<std::string, uint32_t> histogram;
  current::FileSystem::ReadFileByLines("../cooked.csv", [&](std::string line) {
    auto const fields = current::strings::SplitIntoChunks(std::move(line), ',', current::strings::EmptyFields::Keep);
    CURRENT_ASSERT(fields.size() == 23);
    auto const date_fields = current::strings::SplitIntoChunks(fields[1], '-');
    ++histogram[static_cast<std::string>(date_fields[1]) + '/' + static_cast<std::string>(date_fields[0])];
  });

  // Print the counters.
  std::vector<std::pair<std::string, uint32_t>> sorted_histogram(histogram.begin(), histogram.end());
  std::sort(sorted_histogram.begin(), sorted_histogram.end());
  for (auto const& e : sorted_histogram) {
    std::cout << e.second << ' ' << e.first << '\n';
  }
}
