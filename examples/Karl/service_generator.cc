#include "../../Blocks/HTTP/api.h"
#include "../../Bricks/dflags/dflags.h"
#include "../../Karl/test_service/generator.h"

DEFINE_uint16(port, 42001, "The port to spawn ServiceGenerator on.");
DEFINE_double(nps, 10.0, "The `numbers per second` to generate.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  const karl_unittest::ServiceGenerator service(
      FLAGS_port,
      std::chrono::microseconds(static_cast<uint64_t>(1e6 / FLAGS_nps) + 1),
      current::karl::LocalKarl());
  std::cout << "ServiceGenerator up, http://localhost:" << FLAGS_port << "/numbers" << std::endl;
  HTTP(FLAGS_port).Join();
}
