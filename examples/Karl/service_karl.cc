#include "current_build.h"
#include "../../Karl/test_service/is_prime.h"
#include "../../Karl/karl.h"
#include "../../Bricks/dflags/dflags.h"

DEFINE_uint16(nginx_port, 7590, "Port for Nginx to serve proxied queries to Claires.");
DEFINE_string(nginx_config, "", "If set, Karl updates this config with the proxy routes to Claires.");

int main(int argc, char **argv) {
  using namespace current::karl::constants;
  ParseDFlags(&argc, &argv);
  // TODO(dkorolev): Make these paths less non-Windows.
  const current::karl::KarlNginxParameters nginx_parameters(FLAGS_nginx_port, FLAGS_nginx_config);
  const current::karl::GenericKarl<current::karl::default_user_status::status, karl_unittest::is_prime> karl(
      kDefaultKarlPort,
      ".current/stream",
      ".current/storage",
      "/",
      "http://localhost:" + current::ToString(FLAGS_nginx_port),
      "Karl's Example",
      "https://github.com/dkorolev/Current",
      nginx_parameters);
  std::cout << "Karl up, http://localhost:" << kDefaultKarlPort << '/' << std::endl;
  if (!FLAGS_nginx_config.empty()) {
    std::cout << "Karl's Nginx is serving on http://localhost:" << FLAGS_nginx_port << '/' << std::endl;
  }
  HTTP(kDefaultKarlPort).Join();
}
