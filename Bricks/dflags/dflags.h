/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// Header-only command line flags parsing library for C++11. Inspired by Google's gflags. Synopsis:

/*

DEFINE_int32(answer, 42, "Human-readable flag description.");
DEFINE_string(question, "six by nine", "Another human-readable flag description.");

void example() {
  std::cout << FLAGS_question.length() << ' ' << FLAGS_answer * FLAGS_answer << std::endl;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  // Alternatively, `google::ParseCommandLineFlags(&argc, &argv);` is supported for compatibility reasons.
  example();
}

*/

// Supports `string` as `std::string`, int32, uint32, int64, uint64, size_t, float, double and bool.
// Booleans accept 0/1 and lowercase or capitalized true/false/yes/no.
//
// Flags can be passed in as "-flag=value", "--flag=value", "-flag value" or "--flag value" parameters.
// Boolean flags can have the value omitted, in which case the corresponding flag is set to true.
//
// Undefined flag triggers an error message dumped into stderr followed by exit(-1).
// Same happens if `ParseDFlags()` was called more than once.
//
// Non-flag parameters are kept; ParseDFlags() replaces argc/argv with the new, updated values,
// eliminating the ones holding the parsed flags.
// In other words ./main foo --flag_bar=bar baz results in new argc == 2, new argv == { argv[0], "foo", "baz" }.
//
// Passing --help will cause ParseDFlags() to print all registered flags with their descriptions and exit(0).

#ifndef DFLAGS_H
#define DFLAGS_H

#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace dflags {

inline void TerminateExecution(const int code, const std::string& message) {
  std::cerr << message << std::endl;
  std::exit(code);
}

class FlagRegistererBase {
 public:
  virtual ~FlagRegistererBase() {}
  virtual void ParseValue(const std::string& name, const std::string& value) const = 0;
  virtual bool IsBooleanFlag() const = 0;
  virtual std::string TypeAsString() const = 0;
  virtual std::string DefaultValueAsString() const = 0;
  virtual std::string DescriptionAsString() const = 0;
};

class FlagsRegistererSingleton {
 public:
  virtual ~FlagsRegistererSingleton() {}
  virtual void RegisterFlag(const std::string& name, FlagRegistererBase* impl) = 0;
  virtual void ParseFlags(int& argc, char**& argv) = 0;
  void PrintHelpAndExit(const std::map<std::string, FlagRegistererBase*>& flags) const {
    PrintHelp(flags, HelpPrinterOStream());
    std::exit(HelpPrinterReturnCode());
  }

 protected:
  virtual void PrintHelp(const std::map<std::string, FlagRegistererBase*>& flags, std::ostream& os) const {
    os << flags.size() << " flags registered.\n";
    for (const auto cit : flags) {
      os << "\t--" << cit.first << " , " << cit.second->TypeAsString() << "\n\t\t"
         << cit.second->DescriptionAsString() << "\n\t\t"
         << "Default value: " << cit.second->DefaultValueAsString() << '\n';
    }
  }
  // LCOV_EXCL_START -- exclude the following lines from unit test line coverage report.
  virtual std::ostream& HelpPrinterOStream() const { return std::cout; }
  virtual int HelpPrinterReturnCode() const { return 0; }
  // LCOV_EXCL_STOP -- exclude the above lines from unit test line coverage report.
};

template <typename T>
inline bool FromStringSupportingStringAndBool(const std::string& from, T& to) {
  std::istringstream is(from);
  return static_cast<bool>(is >> to);
}

template <>
inline bool FromStringSupportingStringAndBool(const std::string& from, std::string& to) {
  to = from;
  return true;
}

template <>
inline bool FromStringSupportingStringAndBool(const std::string& from, bool& to) {
  if (from == "0" || from == "false" || from == "False" || from == "no" || from == "No") {
    to = false;
    return true;
  } else if (from == "1" || from == "true" || from == "True" || from == "yes" || from == "Yes") {
    to = true;
    return true;
  } else {
    return false;
  }
}

template <typename T>
inline std::string ToStringSupportingStringAndBool(T x) {
  return std::to_string(x);
}

template <>
inline std::string ToStringSupportingStringAndBool(std::string s) {
  return std::string("'") + s + "'";
}

template <>
inline std::string ToStringSupportingStringAndBool(bool b) {
  return b ? "True" : "False";
}

class FlagsManager {
 public:
  class DefaultRegisterer : public FlagsRegistererSingleton {
   public:
    void RegisterFlag(const std::string& name, FlagRegistererBase* impl) override { flags_[name] = impl; }

    void ParseFlags(int& argc, char**& argv) override {
      if (parse_flags_called_) {
        TerminateExecution(-1, "ParseDFlags() is called more than once.");
      }
      parse_flags_called_ = true;
      argv_.push_back(argv[0]);

      for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
          const char* flag = argv[i];
          size_t dashes_count = 0;
          while (*flag == '-') {
            ++flag;
            ++dashes_count;
            if (dashes_count > 2) {
              TerminateExecution(-1,
                                 std::string() + "Parameter: '" + argv[i] + "' has too many dashes in front.");
            }
          }
          // Support both "--flag value" and "--flag=value".
          const char* equals_sign = strstr(flag, "=");
          const std::string flag_name = equals_sign ? std::string(flag, equals_sign) : flag;
          if (flag_name == "help") {
            UserRequestedHelp();
          } else {
            const auto cit = flags_.find(flag_name);
            if (cit == flags_.end()) {
              TerminateExecution(-1, std::string() + "Undefined flag: '" + flag_name + "'.");
            } else {
              const char* flag_value = [&]() {
                // Special case: Enable setting `bool` flags to `true` implicitly, without specifying the value.
                bool tmp_bool;
                if (!equals_sign && cit->second->IsBooleanFlag() &&
                    (i + 1 == argc || !FromStringSupportingStringAndBool(argv[i + 1], tmp_bool))) {
                  return "1";  // Set a `bool` to `true` if it's the past param, or if its "value" is
                               // unparsable.
                } else {
                  return equals_sign ? equals_sign + 1 : (++i, argv[i]);
                }
              }();
              if (i == argc) {
                TerminateExecution(-1,
                                   std::string() + "Flag: '" + flag_name + "' is not provided with the value.");
              }
              cit->second->ParseValue(cit->first, flag_value);
            }
          }
        } else {
          argv_.push_back(argv[i]);
        }
      }

      argc = static_cast<int>(argv_.size());
      argv = &argv_[0];
    }

   private:
    void UserRequestedHelp() { Singleton().PrintHelpAndExit(flags_); }  // LCOV_EXCL_LINE

    std::map<std::string, FlagRegistererBase*> flags_;
    bool parse_flags_called_ = false;
    std::vector<char*> argv_;
  };

  class ScopedSingletonInjector {
   public:
    explicit ScopedSingletonInjector(FlagsRegistererSingleton* ptr)
        : current_ptr_(MockableSingletonGetterAndSetter()) {
      MockableSingletonGetterAndSetter(ptr);
    }
    ~ScopedSingletonInjector() { MockableSingletonGetterAndSetter(current_ptr_); }
    explicit ScopedSingletonInjector(FlagsRegistererSingleton& ref) : ScopedSingletonInjector(&ref) {}

   private:
    FlagsRegistererSingleton* current_ptr_;
  };

  static FlagsRegistererSingleton* MockableSingletonGetterAndSetter(
      FlagsRegistererSingleton* inject_ptr = nullptr) {
    static DefaultRegisterer singleton;
    static FlagsRegistererSingleton* ptr = &singleton;
    if (inject_ptr) {
      ptr = inject_ptr;
    }
    return ptr;
  }

  static FlagsRegistererSingleton& Singleton() { return *MockableSingletonGetterAndSetter(); }

  static void RegisterFlag(const std::string& name, FlagRegistererBase* impl) {
    Singleton().RegisterFlag(name, impl);
  }

  static void ParseFlags(int& argc, char**& argv) { Singleton().ParseFlags(argc, argv); }
};

template <typename FLAG_TYPE>
class FlagRegisterer : public FlagRegistererBase {
 public:
  FlagRegisterer(FLAG_TYPE& ref,
                 const std::string& name,
                 const std::string& type,
                 const FLAG_TYPE default_value,
                 const std::string& description)
      : ref_(ref), name_(name), type_(type), default_value_(default_value), description_(description) {
    FlagsManager::RegisterFlag(name, this);
  }

  virtual void ParseValue(const std::string& name, const std::string& value) const override {
    if (!FromStringSupportingStringAndBool(value, ref_)) {
      TerminateExecution(-1, std::string("Can not parse '") + value + "' for flag '" + name + "'.");
    }
  }

  bool IsBooleanFlag() const override { return std::is_same<FLAG_TYPE, bool>::value; }

  std::string TypeAsString() const override { return type_; }

  std::string DefaultValueAsString() const override { return ToStringSupportingStringAndBool(default_value_); }

  std::string DescriptionAsString() const override { return description_; }

 private:
  FLAG_TYPE& ref_;
  const std::string name_;
  const std::string type_;
  const FLAG_TYPE default_value_;
  const std::string description_;
};

#define DEFINE_flag(type, name, default_value, description) \
  type FLAGS_##name = default_value;                        \
  ::dflags::FlagRegisterer<type> REGISTERER_##name(         \
      std::ref(FLAGS_##name), #name, #type, FLAGS_##name, description)

#define DEFINE_int8(name, default_value, description) DEFINE_flag(int8_t, name, default_value, description)
#define DEFINE_uint8(name, default_value, description) DEFINE_flag(uint8_t, name, default_value, description)
#define DEFINE_int16(name, default_value, description) DEFINE_flag(int16_t, name, default_value, description)
#define DEFINE_uint16(name, default_value, description) DEFINE_flag(uint16_t, name, default_value, description)
#define DEFINE_int32(name, default_value, description) DEFINE_flag(int32_t, name, default_value, description)
#define DEFINE_uint32(name, default_value, description) DEFINE_flag(uint32_t, name, default_value, description)
#define DEFINE_int64(name, default_value, description) DEFINE_flag(int64_t, name, default_value, description)
#define DEFINE_uint64(name, default_value, description) DEFINE_flag(uint64_t, name, default_value, description)
#define DEFINE_size_t(name, default_value, description) DEFINE_flag(size_t, name, default_value, description)
#define DEFINE_float(name, default_value, description) DEFINE_flag(float, name, default_value, description)
#define DEFINE_double(name, default_value, description) DEFINE_flag(double, name, default_value, description)
#define DEFINE_string(name, dflt_value, description) DEFINE_flag(std::string, name, dflt_value, description)
#define DEFINE_bool(name, default_value, description) DEFINE_flag(bool, name, default_value, description)

#ifndef CURRENT_MAKE_CHECK_MODE
#define DECLARE_flag(type, name) extern type FLAGS_##name;
#else
#define DECLARE_flag(type, name) static type FLAGS_##name;  // To have multiple headers link w/o violating ODR.
#endif

#define DECLARE_int8(name) DECLARE_flag(int8_t, name)
#define DECLARE_uint8(name) DECLARE_flag(uint8_t, name)
#define DECLARE_int16(name) DECLARE_flag(int16_t, name)
#define DECLARE_uint16(name) DECLARE_flag(uint16_t, name)
#define DECLARE_int32(name) DECLARE_flag(int32_t, name)
#define DECLARE_uint32(name) DECLARE_flag(uint32_t, name)
#define DECLARE_int64(name) DECLARE_flag(int64_t, name)
#define DECLARE_uint64(name) DECLARE_flag(uint64_t, name)
#define DECLARE_size_t(name) DECLARE_flag(size_t, name)
#define DECLARE_float(name) DECLARE_flag(float, name)
#define DECLARE_double(name) DECLARE_flag(double, name)
#define DECLARE_string(name) DECLARE_flag(std::string, name)
#define DECLARE_bool(name) DECLARE_flag(bool, name)

}  // namespace dflags

inline void ParseDFlags(int* argc, char*** argv) { ::dflags::FlagsManager::ParseFlags(*argc, *argv); }

namespace fake_google {
struct UnambiguousGoogleFriendlyIntPointerWrapper {
  int* p;
  inline UnambiguousGoogleFriendlyIntPointerWrapper(int* p) : p(p) {}
  inline operator int*() { return p; }
};
inline bool ParseCommandLineFlags(UnambiguousGoogleFriendlyIntPointerWrapper argc, char*** argv, bool = true) {
  ParseDFlags(argc, argv);
  return true;
}
}  // namespace fake_google.

using fake_google::ParseCommandLineFlags;

namespace google {
using fake_google::ParseCommandLineFlags;
}

#endif  // DFLAGS_H
