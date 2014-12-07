#include "dflags.h"

DEFINE_int32(answer, 42, "Human-readable flag description.");
DEFINE_string(question, "six by nine", "Another human-readable flag description.");

void example() {
  std::cout << FLAGS_question.length() << ' ' << FLAGS_answer * FLAGS_answer << std::endl;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  example();
}
