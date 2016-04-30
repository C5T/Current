#include "current_build.h"
#include "../../Blocks/HTTP/api.h"
#include "../../Bricks/dflags/dflags.h"
#include "../../Karl/test_service/is_prime.h"

DEFINE_uint16(port, 42002, "The port to spawn ServiceIsPrime on.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  const karl_unittest::ServiceIsPrime service(FLAGS_port, current::karl::LocalKarl());
  std::cout << "ServiceIsPrime up, http://localhost:" << FLAGS_port << "/is_prime?x=43" << std::endl;
  HTTP(FLAGS_port).Join();
}
