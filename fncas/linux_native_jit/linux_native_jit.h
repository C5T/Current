/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// NOTE(dkorolev): This is really an alpha version. "It works on my machines", and then I don't care for now. -- D.K.

#if defined(__linux__) && defined(__x86_64__)

#define FNCAS_LINUX_NATIVE_JIT_ENABLED

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <sys/mman.h>

namespace current {
namespace fncas {
namespace linux_native_jit {

// The signature is:
// * Returns a `double`:              The value of the function.
// * Uses the `double const* x`:      Input, the parameters vector `x[i]`.
// * Uses the `double* o`:            Output, first the gradient, then the temporary memory buffer.
// * Uses the `double (*f[])(double): External functions (`sin`, `exp`, etc.) to be called, to avoid dealing with PLT.
typedef double (*pf_t)(double const* x, double* o, double (*f[])(double));

constexpr static size_t const kPageSize = 4096;

struct CallableVectorUInt8 final {
  size_t const allocated_size_;
  void* buffer_ = nullptr;

  explicit CallableVectorUInt8(std::vector<uint8_t> const& data)
      : allocated_size_(kPageSize * ((data.size() + kPageSize - 1) / kPageSize)) {
    buffer_ = ::mmap(NULL, allocated_size_, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (!buffer_) {
      std::cerr << "`mmap()` failed.\n";
      std::exit(-1);
    }
    if (::mprotect(buffer_, allocated_size_, PROT_READ | PROT_WRITE | PROT_EXEC)) {
      std::cerr << "`mprotect()` failed.\n";
      std::exit(-1);
    }
    ::memcpy(buffer_, reinterpret_cast<void const*>(&data[0]), data.size());
  }

  double operator()(double const* x, double* o, double (*f[])(double)) {
    return reinterpret_cast<pf_t>(buffer_)(x, o, f);
  }

  ~CallableVectorUInt8() {
    if (buffer_) {
      ::munmap(buffer_, allocated_size_);
    }
  }
};

}  // namespace current::fncas::linux_native_jit
}  // namespace current::fncas
}  // namespace current

#endif
