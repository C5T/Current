// To run: make

#include "current.h"

ENDPOINT(data, params) {
  std::map<int, int> histogram;
  for (const IntegerRide& ride : data) {
    ++histogram[ride.pickup.month - 1];
  }
  return histogram;
}
