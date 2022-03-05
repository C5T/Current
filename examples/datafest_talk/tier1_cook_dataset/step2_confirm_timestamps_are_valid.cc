// To run: g++ -std=c++11 -O3 step2_confirm_timestamps_are_valid.cc && time ./a.out
//
// Goes through the cooked CSV and confirms the timestamps constructed from the textual dates are what they should be.

#include <cstdio>
#include <unordered_map>

#include "../../../bricks/strings/strings.h"
#include "../../../bricks/time/chrono.h"

static std::unordered_map<std::string, int> dow_map{
    {"Mon", 0},
    {"Tue", 1},
    {"Wed", 2},
    {"Thu", 3},
    {"Fri", 4},
    {"Sat", 5},
    {"Sun", 6},
};

static const char* const one_based_month_name[13] = {
    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

inline void ValidateTimestamp(std::vector<current::strings::Chunk> const& pieces) {
  CURRENT_ASSERT(pieces.size() == 9);
  auto const year = 2000 + (current::FromString<int>(pieces[0]) % 100);
  auto const month = current::FromString<int>(pieces[1]);
  auto const day = current::FromString<int>(pieces[2]);
  auto const hour = current::FromString<int>(pieces[3]);
  auto const minute = current::FromString<int>(pieces[4]);
  auto const second = current::FromString<int>(pieces[5]);
  std::string const tz = pieces[6];
  std::string const dow_as_string = pieces[7];
  auto const cit = dow_map.find(dow_as_string);
  CURRENT_ASSERT(cit != dow_map.end());
  auto const dow = cit->second;

  auto const epoch_seconds = current::FromString<int>(pieces[8]);

  CURRENT_ASSERT(year >= 2015 && year <= 2017);
  CURRENT_ASSERT(month > 0 && month <= 12);
  CURRENT_ASSERT(day > 0 && day <= 31);
  CURRENT_ASSERT(hour >= 0 && hour < 24);
  CURRENT_ASSERT(minute >= 0 && minute < 60);
  CURRENT_ASSERT(second >= 0 && second < 60);
  CURRENT_ASSERT(tz == "EST" || tz == "EDT");
  CURRENT_ASSERT(dow >= 0 && dow < 7);

  CURRENT_ASSERT(epoch_seconds > 0);

  std::string imf_fix_date = current::strings::Printf("%s, %2d %s %d %02d:%02d:%02d GMT",
                                                      dow_as_string.c_str(),
                                                      day,
                                                      one_based_month_name[month],
                                                      year,
                                                      hour,
                                                      minute,
                                                      second);

  int const imf_fix_timestamp = current::IMFFixDateTimeStringToTimestamp(imf_fix_date).count() / 1000000;

  // The safest and most bulletproof way to test time zones and daylight saving. -- D.K.
  CURRENT_ASSERT((tz == "EST" && imf_fix_timestamp + 3600 * 5 == epoch_seconds) ||
                 (tz == "EDT" && imf_fix_timestamp + 3600 * 4 == epoch_seconds));
}

int main(int argc, char** argv) {
  FILE* f = fopen(argc >= 2 ? argv[1] : "../cooked.csv", "r");
  CURRENT_ASSERT(f);

  char buf[10000];
  while (fgets(buf, sizeof(buf), f)) {
    auto const fields = current::strings::SplitIntoChunks(buf, ',', current::strings::EmptyFields::Keep);

    CURRENT_ASSERT(fields.size() == 23);
    ValidateTimestamp(current::strings::SplitIntoChunks(fields[1], '-'));
    ValidateTimestamp(current::strings::SplitIntoChunks(fields[2], '-'));
  }

  fclose(f);

  printf("All timestamps OK.\n");
}
