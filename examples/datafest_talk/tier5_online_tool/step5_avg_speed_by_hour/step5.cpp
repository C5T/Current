// To run: make

#include "current.h"

struct PerHourCounter {
  int count = 0;
  double sum = 0.0;
  void UpdateAverageSpeed(double x) { ++count; sum += x; }
  double ComputeAverageSpeed() const { return count ? 60.0 * 60.0 * sum / count : 0.0; }
};

FUNCTION(rides, output) {
  PerHourCounter per_hour_counters[24];
  for (const auto& ride : rides) {
    const int trip_duration_seconds = ride.dropoff.epoch - ride.pickup.epoch;
    if (ride.trip_distance_times_100 > 0 && trip_duration_seconds > 0) {
      const double cost_per_mile = 1.0 * ride.fare_amount_cents / ride.trip_distance_times_100;
      if (cost_per_mile >= 2 && cost_per_mile <= 10) {
        per_hour_counters[ride.pickup.hour].UpdateAverageSpeed(
          0.01 * ride.trip_distance_times_100 / trip_duration_seconds);
      }
    }
  }
  for (int hour = 0; hour < 24; ++hour) {
    output << Printf("%02d\t%.2lf\n", hour, per_hour_counters[hour].ComputeAverageSpeed());
  }
}
