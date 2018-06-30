// To run: g++ -std=c++11 -O3 -DNDEBUG step1_service.cc -lpthread -ldl && ./a.out --current_dir=$PWD/../../..

#include "nyc_taxi_dataset_service.h"

#include <cstdio>

#include "../../../blocks/http/api.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/file/file.h"

DEFINE_string(input_dataset, "../cooked_as_integers.bin", "The input binary file to `::mmap` and work with.");
DEFINE_uint16(port, 3000, "The port to serve on.");
DEFINE_string(current_dir, "", "Must be set -- `$PWD/../../..` works -- for user code to be able to use Current.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (FLAGS_current_dir.empty()) {
    fprintf(stderr, "--current_dir must be set, try `--current_dir=$PWD/../../..`.\n");
    return -1;
  }

  const std::string file_body(current::FileSystem::ReadFileAsString(FLAGS_input_dataset));
  const auto* readonly_buffer = file_body.c_str();
  const size_t length = file_body.length();

  if (!((length % sizeof(IntegerRide)) == 0)) {
    fprintf(stderr, "Wrong file size.\n");
    return -1;
  }
  const size_t total_rides = length / sizeof(IntegerRide);

  std::cout << "Operating on " << total_rides << " total rides." << std::endl;

  IterableData iterable_data(reinterpret_cast<const IntegerRide*>(readonly_buffer), total_rides);
  NYCTaxiDatasetService impl(iterable_data, FLAGS_port, FLAGS_current_dir);

  std::cout << "Listening on http://localhost:" << FLAGS_port << '.' << std::endl;

  HTTP(FLAGS_port).Join();
}
