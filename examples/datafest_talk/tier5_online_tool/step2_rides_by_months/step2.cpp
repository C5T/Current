// To run: make

#include "current.h"

FUNCTION(rides, output) {
  std::vector<int> MM(12);

  for (const IntegerRide& ride : rides) {
    ++MM[ride.pickup.month - 1];
  }

  for (int m = 0; m < 12; ++m) {
    output << Printf("%d %02d/2016\n", MM[m], m + 1);
  }
}
