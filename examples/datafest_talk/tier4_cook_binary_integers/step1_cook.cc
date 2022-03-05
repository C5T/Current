// To run: g++ -std=c++11 -O3 step1_cook.cc && time ./a.out

#include <cstdio>
#include <unordered_map>

#include "schema_integers.h"

#include "../../../bricks/strings/strings.h"

std::unordered_map<std::string, uint8_t> dow_map{
    {"Mon", 0},
    {"Tue", 1},
    {"Wed", 2},
    {"Thu", 3},
    {"Fri", 4},
    {"Sat", 5},
    {"Sun", 6},
};

void PackTimestamp(std::vector<current::strings::Chunk> const& pieces, Timestamp& timestamp) {
  CURRENT_ASSERT(pieces.size() == 9);
  timestamp.year = static_cast<uint8_t>(current::FromString<int>(pieces[0]) % 100);
  current::FromString(pieces[1], timestamp.month);
  current::FromString(pieces[2], timestamp.day);
  current::FromString(pieces[3], timestamp.hour);
  current::FromString(pieces[4], timestamp.minute);
  current::FromString(pieces[5], timestamp.second);
  CURRENT_ASSERT(pieces[6].length() == 3);  // "EST" or "EDT".
  timestamp.dst = (pieces[6][1] == 'D');
  auto const cit = dow_map.find(pieces[7]);
  CURRENT_ASSERT(cit != dow_map.end());
  timestamp.dow = cit->second;
  current::FromString(pieces[8], timestamp.epoch);
  CURRENT_ASSERT(timestamp.year >= 15 && timestamp.year <= 17);
  CURRENT_ASSERT(timestamp.month && timestamp.month <= 12);
  CURRENT_ASSERT(timestamp.day && timestamp.day <= 31);
  CURRENT_ASSERT(timestamp.hour < 24);
  CURRENT_ASSERT(timestamp.minute < 60);
  CURRENT_ASSERT(timestamp.second < 60);
}

// Assumes `T` is an integer type.
template <typename T>
void StoreAsInteger(current::strings::Chunk chunk, T& output, double multiplier) {
  output = static_cast<T>(current::FromString<double>(chunk) * multiplier + 0.5);
}

int main() {
  fprintf(stderr, "IntegerRide size in binary: %d bytes.\n", static_cast<int>(sizeof(IntegerRide)));

  FILE* f = fopen("../cooked.csv", "r");
  CURRENT_ASSERT(f);

  std::vector<IntegerRide> records;
  char buf[10000];
  while (fgets(buf, sizeof(buf), f)) {
    auto const fields = current::strings::SplitIntoChunks(buf, ',', current::strings::EmptyFields::Keep);
    CURRENT_ASSERT(fields.size() == 23);

    records.resize(records.size() + 1);
    IntegerRide& record = records.back();

    // Still a nanual, but essential, process. -- D.K.
    CURRENT_ASSERT(fields[0] == "1" || fields[0] == "2");
    record.vendor_id = fields[0][0] - '0';

    PackTimestamp(current::strings::SplitIntoChunks(fields[1], '-'), record.pickup);
    PackTimestamp(current::strings::SplitIntoChunks(fields[2], '-'), record.dropoff);

    CURRENT_ASSERT(fields[3] == "N" || fields[3] == "Y");
    record.store_and_fwd_flag = fields[3][0];

    current::FromString(fields[4], record.ratecode_id);

    StoreAsInteger(fields[5], record.pickup_longitude_times_1m, 1e6);
    StoreAsInteger(fields[6], record.pickup_latitude_times_1m, 1e6);
    StoreAsInteger(fields[7], record.dropoff_longitude_times_1m, 1e6);
    StoreAsInteger(fields[8], record.dropoff_longitude_times_1m, 1e6);

    current::FromString(fields[9], record.passenger_count);
    StoreAsInteger(fields[10], record.trip_distance_times_100, 100);
    StoreAsInteger(fields[11], record.fare_amount_cents, 100);
    StoreAsInteger(fields[12], record.extra_cents, 100);
    StoreAsInteger(fields[13], record.mta_tax_cents, 100);
    StoreAsInteger(fields[14], record.tip_amount_cents, 100);
    StoreAsInteger(fields[15], record.tolls_amount_cents, 100);

    CURRENT_ASSERT(fields[16] == "");  // `e_hail_fee`, always empty.

    StoreAsInteger(fields[17], record.improvement_surcharge_cents, 100);
    StoreAsInteger(fields[18], record.total_amount_cents, 100);
    current::FromString(fields[19], record.payment_type);

    CURRENT_ASSERT(fields[20] == "" || fields[20] == "1" || fields[20] == "2");
    current::FromString(fields[20], record.trip_type);

    current::FromString(fields[21], record.pu_location_id);
    current::FromString(fields[22], record.do_location_id);
  }

  fclose(f);

  FILE* g = fopen("../cooked_as_integers.bin", "wb");
  CURRENT_ASSERT(g);
  CURRENT_ASSERT(fwrite(&records[0], sizeof(IntegerRide), records.size(), g) == records.size());
  fclose(g);
}
