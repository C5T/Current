/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * *******************************************************************************/

#ifndef FNCAS_FNCAS_BASE_H
#define FNCAS_FNCAS_BASE_H

#include "../../bricks/port.h"

#include <vector>
#include <cstdlib>
#include <cstdint>

#include "../x64_native_jit/x64_native_jit.h"

namespace fncas {

#ifndef FNCAS_USE_LONG_DOUBLE
typedef double double_t;  // Make it simple to switch to `long double` or a custom type.
#else
typedef long double double_t;
#endif

namespace impl {

using node_index_t = int64_t;  // Allow 4B+ nodes on 64-bit arch, keep signed for (~i) vs. (i) index magic.

struct noncopyable {
  noncopyable() = default;
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;
  noncopyable(noncopyable&&) = delete;
  void operator=(noncopyable&&) = delete;
};

template <typename VECTOR_TYPE, typename FILL_VALUE_TYPE>
VECTOR_TYPE& growing_vector_access(std::vector<VECTOR_TYPE>& vector, node_index_t index, FILL_VALUE_TYPE fill) {
  if (static_cast<node_index_t>(vector.size()) <= index) {
    vector.resize(static_cast<size_t>(index + 1), fill);
  }
  return vector[static_cast<size_t>(index)];
}

enum class NodeType : uint8_t { variable, value, operation, function };
enum class MathOperation : uint8_t { add, subtract, multiply, divide, end };
enum class MathFunction : uint8_t {
#define FNCAS_FUNCTION(f) f,
#include "fncas_functions.dsl.h"
#undef FNCAS_FUNCTION
  end
};

enum class JIT {
  Super,          // The superclass of all `function_t<>`-s and `gradient_t<>`-s respectively.
  Blueprint,      // The blueprint of the function of gradient, evaluated by traversing an internal tree.
  NativeWrapper,  // A wrapper over an `std::function<double(std::vector<double>&)>`, or approximate gradient computer.
  CLANG,          // JIT via `clang++`.
  AS,             // JIT via `as`.
  NASM,           // JIT via `nasm`.
#ifndef FNCAS_X64_NATIVE_JIT_ENABLED
  // The JIT used by default by the optimization algorithms is the `as` one ...
  Default = AS
#else
  // ... unless we are on a platform where the x64 native JIT is available (Linux or MacOS).
  X64NativeJIT,
  Default = X64NativeJIT
#endif
};

template <JIT>
class JITImplementation;

enum class OptimizationDirection { Minimize, Maximize };

}  // namespace fncas::impl

using impl::JIT;
using impl::OptimizationDirection;

}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_BASE_H
