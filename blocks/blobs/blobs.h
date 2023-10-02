/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2023 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_BLOCKS_BLOBS_BLOBS_H
#define CURRENT_BLOCKS_BLOBS_BLOBS_H

#include "../../port.h"

#include <iostream>

#include "bricks/file/file.h"
#include "typesystem/reflection/typeid.h"

namespace current {

template <class T>
void WriteBlob(const std::vector<T>& data, const std::string& filename) {
  std::ofstream file(filename, std::ios::binary);
  const auto signature = current::reflection::CurrentTypeID<T>();
  file.write(reinterpret_cast<const char*>(&signature), sizeof(signature));
  file.write(reinterpret_cast<const char*>(&data[0]), sizeof(T) * data.size());
}

template <class T, class F>
void ProcessBlob(const std::string& filename, F&& f) {
  const std::string contents = current::FileSystem::ReadFileAsString(filename);
  const auto signature = *reinterpret_cast<const current::reflection::TypeID*>(contents.c_str());
  if (signature != current::reflection::CurrentTypeID<T>()) {
    throw current::Exception("Wrong type.");
  }
  size_t length = contents.length();
  size_t n = (length - sizeof(signature)) / sizeof(T);
  if (sizeof(signature) + n * sizeof(T) != length) {
    throw current::Exception("Wrong file size.");
  }
  // TODO(dkorolev): Add a wrapper class so that it has `.size()`, iteration via `for (element : elements)`, etc.
  f(reinterpret_cast<const T*>(contents.c_str() + sizeof(signature)), n);
}

}  // namespace current

#endif  // CURRENT_BLOCKS_BLOBS_BLOBS_H
