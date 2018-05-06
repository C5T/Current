/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_SYSTEM_SYSCALLS_H
#define BRICKS_SYSTEM_SYSCALLS_H

#include "../../port.h"

#include "../exception.h"
#include "../file/file.h"
#include "../strings/util.h"

#include <dlfcn.h>

#include <cstdio>
#include <vector>

namespace current {
namespace bricks {
namespace system {

struct SystemException : Exception {
  using Exception::Exception;
};

struct PopenCallException final : SystemException {
  using SystemException::SystemException;
};

struct SystemCallException final : SystemException {
  using SystemException::SystemException;
};

class InputTextPipe final {
 private:
  const std::string command_;
  std::vector<char> buffer_;
  FILE* f_;

 public:
  template <typename S>
  explicit InputTextPipe(S&& command, size_t max_line_length = 1024 * 1024)
      : command_(std::forward<S>(command)), buffer_(max_line_length), f_(::popen(command_.c_str(), "r")) {
    if (!f_) {
      // NOTE(dkorolev): I couldn't test it.
      CURRENT_THROW(PopenCallException(command_));
    }
  }

  ~InputTextPipe() {
    if (f_) {
      ::pclose(f_);
    }
  }

  // Read a single line, or, if can not, returns an empty string.
  // Generally, if an empty string was returned from a valid `InputTextPipe`,
  // casting it to `bool` is the way to tell whether the stream of data to read has ended.
  std::string ReadLine() {
    if (f_) {
      const char* p = ::fgets(&buffer_[0], buffer_.size(), f_);
      if (!p) {
        ::pclose(f_);
        f_ = nullptr;
        return "";
      } else {
        std::string s(p);
        if (!s.empty() && s.back() == '\n') {
          s.resize(s.length() - 1u);
        }
        return s;
      }
    } else {
      return "";
    }
  }
  // `false` if the `popen()` call failed, or is the previous `fgets()` call returned a null pointer.
  operator bool() const { return f_; }
};

template <typename S>
int SystemCall(S&& input_command) {
  const std::string command(std::forward<S>(input_command));
  int result = ::system(command.c_str());
  if (result == -1 || result == 127) {
    if (!::system("")) {
      CURRENT_THROW(SystemCallException("No shell available to call `::system()`."));
    } else if (result == -1) {
      CURRENT_THROW(SystemCallException("Child process could not be created for: " + command));
    } else if (result == 127) {
      CURRENT_THROW(SystemCallException("Child process could not start the shell for: " + command));
    }
  }
  return result;
}

struct ExternalLibraryException : SystemException {
  using SystemException::SystemException;
};

struct CompilationException final : ExternalLibraryException {
  using ExternalLibraryException::ExternalLibraryException;
};

struct DLOpenException final : ExternalLibraryException {
  using ExternalLibraryException::ExternalLibraryException;
};

struct DLSymException final : ExternalLibraryException {
  using ExternalLibraryException::ExternalLibraryException;
};

class JITCompiledCPP final {
 private:
  const std::string base_file_name_;
  const std::string source_file_name_;
  const std::string library_file_name_;
  const current::FileSystem::ScopedRmFile source_file_remover_;
  const current::FileSystem::ScopedRmFile library_file_remover_;
  void* lib_ = nullptr;

 public:
  template <typename S>
  explicit JITCompiledCPP(S&& source)
      : base_file_name_(current::FileSystem::GenTmpFileName()),
        source_file_name_(base_file_name_ + ".cc"),
        library_file_name_(base_file_name_ + ".so"),
        source_file_remover_(source_file_name_),
        library_file_remover_(library_file_name_) {
    current::FileSystem::WriteStringToFile(current::strings::ConstCharPtr(std::forward<S>(source)),
                                           source_file_name_.c_str());
    std::string cmdline = "g++ -fPIC -shared -nostartfiles " + source_file_name_ + " -o " + library_file_name_;

#if 1
    // Try to capture the compilation error message into the exception body.
    // Testing how cross-platform this is. -- D.K.
    cmdline += " 2>&1";
    std::ostringstream error_message;
    bool compilation_successful = true;
    InputTextPipe pipe(cmdline);
    do {
      std::string error_message_line = pipe.ReadLine();
      if (!error_message_line.empty()) {
        error_message << error_message_line << '\n';
        compilation_successful = false;
      }
    } while (pipe);
    if (!compilation_successful) {
      CURRENT_THROW(CompilationException("Failed to compile user code:\n" + error_message.str()));
    }
#else
    // The default implementation.
    if (current::bricks::system::SystemCall(cmdline.c_str())) {
      CURRENT_THROW(CompilationException("Failed to compile user code."));
    }
#endif

    lib_ = ::dlopen(library_file_name_.c_str(), RTLD_LAZY);
    if (!lib_) {
      CURRENT_THROW(DLOpenException("Failed to load the library."));
    }
  }

  ~JITCompiledCPP() {
    if (lib_) {
      ::dlclose(lib_);
    }
  }

  template <typename F, typename S>
  F Get(S&& function_name) {
    if (!lib_) {
      CURRENT_THROW(DLOpenException("Failed to load the library."));
    } else {
      void* f = ::dlsym(lib_, current::strings::ConstCharPtr(std::forward<S>(function_name)));
      if (!f) {
        CURRENT_THROW(DLSymException("Failed to load the symbol."));
      } else {
        return reinterpret_cast<F>(f);
      }
    }
  }
};

}  // namespace current::bricks::system
}  // namespace current::bricks
}  // namespace current

#endif  // BRICKS_SYSTEM_SYSCALLS_H
