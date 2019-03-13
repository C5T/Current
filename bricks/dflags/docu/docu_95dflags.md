## Command Line Parsing: `dflags`

Bricks has [`dflags`](https://github.com/C5T/Current/blob/stable/bricks/dflags/dflags.h): a C++ library to parse command-line flags.

```cpp
DEFINE_int32(answer, 42, "Human-readable flag description.");
DEFINE_string(question, "six by nine", "Another human-readable flag description.");

void example() {
  std::cout << FLAGS_question.length() << ' ' << FLAGS_answer * FLAGS_answer << std::endl;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  // `google::ParseCommandLineFlags(&argc, &argv);`
  // is supported as well for compatibility reasons.
  example();
}
```

Supported types are `string` as `std::string`, `int32`, `uint32`, `int64`, `uint64`, `float`, `double` and `bool`. Booleans accept `0`/`1` and lowercase or capitalized `true`/`false`/`yes`/`no`.

Flags can be passed in as `-flag=value`, `--flag=value`, `-flag value` or `--flag value` parameters.

Undefined flag triggers an error message dumped into stderr followed by exit(-1).  Same happens if `ParseDFlags()` was called more than once.

Non-flag parameters are kept; ParseDFlags() replaces argc/argv with the new, updated values, eliminating the ones holding the parsed flags. In other words `./main foo --flag_bar=bar baz` results in new `argc == 2`, new `argv == { argv[0], "foo", "baz" }`.

Passing `--help` will cause `ParseDFlags()` to print all registered flags with their descriptions and `exit(0)`.

[`dflags`](https://github.com/KnowSheet/bricks/blob/stable/dflags/dflags.h) is a simplified header-only version of Google's [`gflags`](https://code.google.com/p/gflags/). It requires no linker dependencies and largely is backwards-compatible.
