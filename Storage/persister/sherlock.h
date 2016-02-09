/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_PERSISTER_SHERLOCK_H
#define CURRENT_STORAGE_PERSISTER_SHERLOCK_H

#include "../base.h"
#include "../exceptions.h"
#include "../../Sherlock/sherlock.h"

namespace current {
namespace storage {
namespace persister {

template <typename TYPELIST, template <typename, typename> class PERSISTER, typename CLONER>
class SherlockStreamPersisterImpl;

template <template <typename, typename> class PERSISTER, typename CLONER, typename... TS>
class SherlockStreamPersisterImpl<TypeList<TS...>, PERSISTER, CLONER> {
 public:
  using T_VARIANT = Variant<TS...>;
  using T_RECORD = std::pair<std::vector<T_VARIANT>, std::chrono::microseconds>;

  template <typename... ARGS>
  explicit SherlockStreamPersisterImpl(ARGS&&... args)
      : stream_(sherlock::Stream<T_RECORD, PERSISTER, CLONER>(std::forward<ARGS>(args)...)) {}

  void PersistJournal(MutationJournal& journal) {
    T_RECORD record;
    for (auto&& entry : journal.commit_log) {
      record.first.emplace_back(std::move(entry));
    }
    record.second = current::time::Now();
    stream_.Publish(std::move(record));
    journal.commit_log.clear();
    journal.rollback_log.clear();
  }

  template <typename F>
  void Replay(F&& f) {
    // TODO(dkorolev) + TODO(mzhurovich): Perhaps `Replay()` should happen automatically,
    // during construction, in a blocking way?
    static_cast<void>(f);
  }

 private:
  sherlock::StreamInstance<T_RECORD, PERSISTER, CLONER> stream_;
};

template <typename TYPELIST>
using SherlockInMemoryStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, blocks::persistence::MemoryOnly, current::DefaultCloner>;

template <typename TYPELIST>
using SherlockStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, blocks::persistence::NewAppendToFile, current::DefaultCloner>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::SherlockInMemoryStreamPersister;
using current::storage::persister::SherlockStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_SHERLOCK_H
