#include "dflags.h"

DEFINE_int32(answer, 42, "Human-readable flag description.");
DEFINE_string(question, "six by nine", "Another human-readable flag description.");

void example() {
  std::cout << FLAGS_question.length() << ' ' << FLAGS_answer * FLAGS_answer << std::endl;
}

namespace google {
void ParseCommandLineFlags(int* argc, char*** argv) {
  std::cout << "Although another `google::ParseCommandLineFlags()` implementation exists," << std::endl
            << "DFlags still compiles. However, since this another implementation is dummy," << std::endl
            << "this binary is not parsing flags or `--help` from the command line." << std::endl;
}
}  // namespace google

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv);
  example();
}
