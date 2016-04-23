#include "../../Blocks/HTTP/api.h"
#include "../../Karl/karl.h"

int main() {
  using namespace current::karl::constants;
  const current::karl::Karl karl(kDefaultKarlPort);
  std::cout << "Karl up, http://localhost:" << kDefaultKarlPort << '/' << std::endl;
  HTTP(kDefaultKarlPort).Join();
}
