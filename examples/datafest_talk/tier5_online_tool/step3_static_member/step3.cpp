// To run: make

#include "current.h"

static int counter = 0;

FUNCTION(rides, output) {
  output << "Analyzing " << rides.size() << " rides, called " << ++counter << " times.\n";
}
