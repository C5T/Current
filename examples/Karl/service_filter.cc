#include "current_build.h"
#include "../../Blocks/HTTP/api.h"
#include "../../Bricks/dflags/dflags.h"
#include "../../Karl/test_service/filter.h"

DEFINE_uint16(port, 42004, "The port to spawn ServiceAnnotator on.");
DEFINE_string(annotator, "http://localhost:42003", "The route to `ServiceAnnotator`.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  const karl_unittest::ServiceFilter service(FLAGS_port, FLAGS_annotator, current::karl::LocalKarl());
  std::cout << "ServiceFilter up, http://localhost:" << FLAGS_port << "/annotated" << std::endl;
  HTTP(FLAGS_port).Join();
}
