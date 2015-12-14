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

#ifndef CURRENT_TRANSACTIONAL_STORAGE_PERSISTER_FILE_H
#define CURRENT_TRANSACTIONAL_STORAGE_PERSISTER_FILE_H

#include <fstream>

#include "../base.h"
#include "../exceptions.h"
#include "../../TypeSystem/Serialization/json.h"

namespace current {
namespace storage {
namespace persister {

template <typename... TS>
struct JSONFilePersister;

template <typename... TS>
struct JSONFilePersister<TypeList<TS...>> {
  using T_VARIANT = Variant<TS...>;
  explicit JSONFilePersister(const std::string& filename) : filename_(filename) {}

  void PersistJournal(MutationJournal& journal) {
    std::ofstream os(filename_, std::fstream::app);
    if (!os.good()) {
      throw StorageCannotAppendToFileException(filename_);
    }
    for (auto&& entry : journal.commit_log) {
      os << JSON(T_VARIANT(std::move(entry))) << '\n';
    }
    journal.commit_log.clear();
    journal.rollback_log.clear();
  }

  template <typename F>
  void Replay(F&& f) {
    std::ifstream is(filename_);
    if (is.good()) {
      for (std::string line; std::getline(is, line);) {
        f(std::move(ParseJSON<T_VARIANT>(line)));
      }
    }
  }

 private:
  std::string filename_;
};

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::JSONFilePersister;

#endif  // CURRENT_TRANSACTIONAL_STORAGE_PERSISTER_FILE_H
