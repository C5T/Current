#include "impl.h"

#include "../../blocks/http/api.h"
#include "../../bricks/dflags/dflags.h"

DEFINE_string(input_filename, "../iris/data/dataset.json", "The input Irises dataset.");
DEFINE_uint16(port, 3000, "The port to serve on.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  DynamicDLOpenIrisExampleImpl impl(FLAGS_input_filename, FLAGS_port);
  std::cout << "Working on " << impl.TotalFlowers() << " Iris flowers, listening on http://localhost:" << FLAGS_port
            << "." << std::endl;
  HTTP(FLAGS_port).Join();
}
