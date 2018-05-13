#include <sstream>
#include <iomanip>
#include <vector>

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "current/blocks/http/api.h"

struct Timestamp {
  uint8_t year;  // Modulo 100.
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t dst;  // `0` or `1`.
  uint8_t dow;
  int epoch;
};

#ifdef __cplusplus
static_assert(sizeof(Timestamp) == 12, "");
#endif

// Note that:
// 1) No floating point in sight.
// 2) The `e_hail_fee` field is gone (it was always an empty string regardless).
// 3) The fields are re-arranged to pack the one-bytes ones together, at the end.
struct IntegerRide {
  struct Timestamp pickup;
  struct Timestamp dropoff;
  int32_t pickup_longitude_times_1m;
  int32_t pickup_latitude_times_1m;
  int32_t dropoff_longitude_times_1m;
  int32_t dropoff_latitude_times_1m;
  int32_t trip_distance_times_100;  // Trip distances are rounted to 0.01 miles, easier to divide later. -- D.K.
  int32_t fare_amount_cents;        // All amounts are in cents.
  int32_t extra_cents;
  int32_t mta_tax_cents;
  int32_t tip_amount_cents;
  int32_t tolls_amount_cents;
  int32_t improvement_surcharge_cents;
  int32_t total_amount_cents;
  uint8_t vendor_id;                // Same 1 or 2.
  char store_and_fwd_flag;          // Same 'N' or 'Y'.
  uint8_t ratecode_id;              // Integer, { 1, 2, 3, 4, 5, 6, 99 }.
  uint8_t passenger_count;          // Integer, { 0 .. 9 }.
  uint8_t payment_type;             // One of { 1, 2, 3, 4, 5 }.
  uint8_t trip_type;                // Empty = 0, '1', or '2'.
  uint16_t pu_location_id;
  uint16_t do_location_id;
};

struct IterableData {
  const IntegerRide* data_buffer_;
  const size_t total_rides_;
  IterableData(const IntegerRide* data_buffer, size_t total_rides) : data_buffer_(data_buffer), total_rides_(total_rides) {}
  const IntegerRide* begin() const {
    return data_buffer_;
  }
  const IntegerRide* end() const {
    return data_buffer_ + total_rides_;
  }
  size_t size() const { return total_rides_; }
};

#define ENDPOINT(data, params)                                                                \
  inline Response DoRun(const IterableData& data, std::map<std::string, std::string> params); \
  extern "C" void Run(const IterableData& data, Request request) {                            \
    request(DoRun(data, request.url.query.AsImmutableMap()));                                 \
  }                                                                                           \
  Response DoRun(const IterableData& data, std::map<std::string, std::string> params)
