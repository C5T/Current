/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
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

#ifndef FNCAS_FNCAS_EXCEPTIONS_H
#define FNCAS_FNCAS_EXCEPTIONS_H

#include "../../bricks/exception.h"

namespace fncas {
namespace exceptions {

struct FnCASException : current::Exception {
  using super_t = current::Exception;
  using super_t::super_t;
};

// This exception is thrown when more than one expression per thread
// is attempted to be evaluated concurrently under FnCAS.
// This is not allowed. FnCAS keeps global state per thread, which leads to this constraint.
struct FnCASConcurrentEvaluationAttemptException : FnCASException {};

// Exceptions for attempting to differentiate a function that doesn't have a derivative.
struct FnCASFunctionNonDifferentiable : FnCASException {};
struct FnCASZeroOrOneIsNonDifferentiable : FnCASFunctionNonDifferentiable {};

// This exception is thrown when the optimization process fails,
// most notably when the objective function becomes not `fncas::IsNormal()`, which is NaN.
struct FnCASOptimizationException : FnCASException {
  using FnCASException::FnCASException;
};

// This exception is thrown when the call to `Backtracking` in `mathutil.h` results in no viable step found.
// It's generally an error, but a particulal optimization algorithms may choose to handle it.
struct BacktrackingException : FnCASOptimizationException {
  using FnCASOptimizationException::FnCASOptimizationException;
};

}  // namespace exceptions
}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_EXCEPTIONS_H
