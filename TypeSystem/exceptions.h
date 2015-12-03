/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_EXCEPTIONS_H
#define CURRENT_TYPE_SYSTEM_EXCEPTIONS_H

#include "../Bricks/exception.h"

namespace current {

struct NoValueException : Exception {};
template <typename T>
struct NoValueOfTypeException : NoValueException {};

typedef const NoValueException& NoValue;

template <typename T>
struct NoValueOfTypeExceptionWrapper {
  using underlying_type = NoValueOfTypeException<T>;
  typedef const underlying_type& const_reference_type;
};

template <typename T>
using NoValueOfType = typename NoValueOfTypeExceptionWrapper<T>::const_reference_type;

struct UninitializedRequiredVariantException : Exception {};
template <typename... TS>
struct UninitializedRequiredVariantOfTypeException : UninitializedRequiredVariantException {};

typedef const UninitializedRequiredVariantException& UninitializedRequiredVariant;

template <typename... TS>
struct UninitializedRequiredVariantOfTypeExceptionWrapper {
  using underlying_type = UninitializedRequiredVariantOfTypeException<TS...>;
  typedef const underlying_type& const_reference_type;
};

template <typename... TS>
using UninitializedRequiredVariantOfType =
    typename UninitializedRequiredVariantOfTypeExceptionWrapper<TS...>::const_reference_type;

}  // namespace current

using current::NoValueException;
using current::NoValueOfTypeException;
using current::UninitializedRequiredVariantException;
using current::UninitializedRequiredVariantOfTypeException;

using current::NoValue;        // == `const NoValueException&` for cleaner `catch (NoValue)` syntax.
using current::NoValueOfType;  // == `const NoValueOfTypeException<T>&` for cleaner `catch ()` syntax.
using current::UninitializedRequiredVariant;        // == a const reference to `UninitializedRequiredVariant`.
using current::UninitializedRequiredVariantOfType;  // == a const reference as well.

#endif  // CURRENT_TYPE_SYSTEM_EXCEPTIONS_H
