// To run: make

#include "current.h"

CURRENT_STRUCT(Result) {
  CURRENT_FIELD(params, (std::map<std::string, std::string>));
  CURRENT_FIELD(histogram, (std::map<std::string, int>));
};

ENDPOINT(data, params) {
  Result result;
  result.params = params;
  for (const IntegerRide& ride : data) {
    ++result.histogram[current::ToString(ride.pickup.month)];
  }
  return result;
}
