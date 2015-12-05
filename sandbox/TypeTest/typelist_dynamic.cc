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

#include "../../TypeSystem/struct.h"

#include "../../Bricks/template/mapreduce.h"
#include "../../Bricks/template/typelist.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace type_test {

#include "include/typelist_dynamic.h"

}  // namespace type_test

template <typename T>
using ST = TypeListImpl<typename T::Add, typename T::Delete>;

TEST(TypeTest, TypeListImpl) {
  using namespace type_test;
#include "include/typelist_dynamic.cc"

  using current::metaprogramming::map;
  using current::metaprogramming::Flatten;
  typedef Flatten<map<ST, DATA_TYPES>> STORAGE_TYPES;

  static_assert(IsTypeList<STORAGE_TYPES>::value, "");
}
