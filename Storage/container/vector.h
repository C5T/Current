/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STORAGE_CONTAINER_VECTOR_H
#define CURRENT_STORAGE_CONTAINER_VECTOR_H

#include "../base.h"

#include "../../TypeSystem/optional.h"

namespace current {
namespace storage {
namespace container {

struct CannotPopBackFromEmptyVectorException : Exception {};
typedef const CannotPopBackFromEmptyVectorException& CannotPopBackFromEmptyVector;

template <typename T, typename ADDER, typename DELETER>
class Vector {
 public:
  explicit Vector(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return vector_.empty(); }
  size_t Size() const { return vector_.size(); }

  ImmutableOptional<T> operator[](size_t index) const {
    if (index < vector_.size()) {
      return ImmutableOptional<T>(FromBarePointer(), &vector_[index]);
    } else {
      return ImmutableOptional<T>(nullptr);
    }
  }

  void PushBack(const T& object) {
    journal_.LogMutation(ADDER(object), [this]() { vector_.pop_back(); });
    vector_.push_back(object);
  }

  void PopBack() {
    if (!vector_.empty()) {
      auto back_object = vector_.back();
      journal_.LogMutation(DELETER(back_object), [this, back_object]() { vector_.push_back(back_object); });
      vector_.pop_back();
    } else {
      CURRENT_THROW(CannotPopBackFromEmptyVectorException());
    }
  }

  void operator()(const ADDER& object) { vector_.push_back(object); }

  void operator()(const DELETER&) { vector_.pop_back(); }

 private:
  std::vector<T> vector_;
  MutationJournal& journal_;
};

}  // namespace container
}  // namespace storage
}  // namespace current

using current::storage::container::Vector;

#endif  // CURRENT_STORAGE_CONTAINER_VECTOR_H
