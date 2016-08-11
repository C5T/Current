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

#include "../../Storage/storage.h"
#include "../../Storage/persister/sherlock.h"

#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace type_test {

#include "include/storage.h"

}  // namespace type_test

TEST(TypeTest, Storage) {
  using namespace type_test;

  using storage_t = Storage<SherlockStreamPersister>;
  using transaction_t = typename storage_t::transaction_t;

  const auto persistence_file_remover = current::FileSystem::ScopedRmFile("data");

  storage_t storage("data");

#include "include/storage.cc"

  struct Subscriber {
    size_t number_of_mutations = 0u;

    current::ss::EntryResponse operator()(const transaction_t& transaction, idxts_t, idxts_t) {
      assert(!number_of_mutations);
      assert(!transaction.mutations.empty());
      number_of_mutations = transaction.mutations.size();
      return current::ss::EntryResponse::Done;
    }

    static current::ss::EntryResponse EntryResponseIfNoMorePassTypeFilter() {
      return current::ss::EntryResponse::More;
    }

    current::ss::TerminationResponse Terminate() { return current::ss::TerminationResponse::Wait; }
  };

  current::ss::StreamSubscriber<Subscriber, transaction_t> subscriber;
  storage.InternalExposeStream().Subscribe(subscriber);

  EXPECT_EQ(static_cast<size_t>(storage_t::FIELDS_COUNT), subscriber.number_of_mutations);
}
