#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

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

// NOTE(dkorolev): Deliberately not re-arranging any fields.
// NOTE(dkorolev): Deliberately not converting floating points into integers.

struct Ride {
  uint8_t vendor_id;  // 1 or 2.
  struct Timestamp pickup;
  struct Timestamp dropoff;
  char store_and_fwd_flag;  // 'N' or 'Y'.
  uint8_t ratecode_id;      // Integer, { 1, 2, 3, 4, 5, 6, 99 }.
  float pickup_longitude;
  float pickup_latitude;
  float dropoff_longitude;
  float dropoff_latitude;
  uint8_t passenger_count;      // Integer, { 0 .. 9 }.
  float trip_distance;          // Float, most commonly 0, followed by `0.9`, `1`, `0.8`, `1.1`, `1.2`, etc.
  float fare_amount;            // Float, most commonly 6, then 5.5.
  float extra;                  // Float, small, sometimes negative.
  float mta_tax;                // Float, small, sometimes negative.
  float tip_amount;             // Float, sometimes negative.
  float tolls_amount;           // Float, sometimes negative.
  uint8_t e_hail_fee;           // Always an empty string, looks like.
  float improvement_surcharge;  // A small float, most commonly 0, 0.3, and -0.3.
  float total_amount;           // Float.
  uint8_t payment_type;         // One of { 1, 2, 3, 4, 5 }.
  uint8_t trip_type;            // Empty, '1', or '2'.
  uint8_t pu_location_id;       // Integer, small, but can be greater than 256.
  uint8_t do_location_id;       // A small integer.
};
