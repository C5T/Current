#include "../../Blocks/HTTP/api.h"
#include "../../Bricks/dflags/dflags.h"
#include "../../Karl/service_annotator.h"

DEFINE_uint16(port, 42003, "The port to spawn ServiceAnnotator on.");
DEFINE_string(generator, "http://localhost:42001/numbers", "The route to `ServiceGenerator`.");
DEFINE_string(is_prime, "http://localhost:42002/is_prime", "The route to `ServiceIsPrime`.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  const karl_unittest::ServiceAnnotator service(
      FLAGS_port, FLAGS_generator, FLAGS_is_prime, current::karl::LocalKarl());
  std::cout << "ServiceAnnotator up, http://localhost:" << FLAGS_port << "/annotated" << std::endl;
  HTTP(FLAGS_port).Join();
}
