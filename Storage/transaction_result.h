/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_TRANSACTION_RESULT_H
#define CURRENT_STORAGE_TRANSACTION_RESULT_H

#include "../TypeSystem/exceptions.h"

namespace current {
namespace storage {

template <typename T>
struct TransactionResult {
  TransactionResult() : successful_(false) {}

  TransactionResult(TransactionResult&& rhs) : value_(std::move(rhs.value_)), successful_(rhs.successful_) {}

  TransactionResult<T>& operator=(TransactionResult&& rhs) {
    value_ = std::move(rhs.value_);
    successful_ = rhs.successful_;
    return *this;
  }

  TransactionResult(T&& result) : value_(std::move(result)), successful_(true) {}

  TransactionResult<T>& operator=(T&& result) {
    value_ = std::move(result);
    successful_ = true;
    return *this;
  }

  TransactionResult(const TransactionResult&) = delete;
  TransactionResult<T>& operator=(const TransactionResult&) = delete;

  bool SuccessfulImpl() const { return successful_; }

  T&& ValueImpl() {
    if (successful_) {
      return std::move(value_);
    } else {
      throw NoValueOfTypeException<T>();
    }
  }

  const T& ValueImpl() const {
    if (successful_) {
      return value_;
    } else {
      throw NoValueOfTypeException<T>();
    }
  }

 private:
  bool successful_;
  T value_;
};

template <>
struct TransactionResult<void> {
  explicit TransactionResult(bool successful) : successful_(successful) {}

  TransactionResult(TransactionResult&& rhs) : successful_(rhs.successful_) {}

  TransactionResult<void>& operator=(TransactionResult&& rhs) {
    successful_ = rhs.successful_;
    return *this;
  }

  TransactionResult(const TransactionResult&) = delete;
  TransactionResult<void>& operator=(const TransactionResult&) = delete;

  bool SuccessfulImpl() const { return successful_; }

 private:
  bool successful_;
};

}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_TRANSACTION_RESULT_H
