#include <vector>
#include <sstream>
#include <iomanip>

struct Base {
  virtual ~Base() {}
};

struct Flower : Base {
  virtual ~Flower() {}
  union {
    struct {
      double sl;
      double sw;
      double pl;
      double pw;
    };
    double x[4];
  };
  std::string label;
};

extern "C" void Run(const std::vector<Flower>& flowers, std::ostringstream& os) {
