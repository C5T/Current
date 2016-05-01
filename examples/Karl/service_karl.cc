#include "current_build.h"
#include "../../Blocks/HTTP/api.h"
#include "../../Karl/karl.h"

int main() {
  using namespace current::karl::constants;
  // TODO(dkorolev): Make these paths less non-Windows.
  const current::karl::Karl karl(kDefaultKarlPort, ".current/stream", ".current/storage");
  std::cout << "Karl up, http://localhost:" << kDefaultKarlPort << '/' << std::endl;
  HTTP(kDefaultKarlPort).Join();
}
