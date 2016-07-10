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

#ifndef CURRENT_STORAGE_PERSISTER_FILE_H
#define CURRENT_STORAGE_PERSISTER_FILE_H

#include <fstream>

#include "common.h"
#include "../base.h"
#include "../exceptions.h"
#include "../../TypeSystem/Serialization/json.h"

namespace current {
namespace storage {
namespace persister {

template <typename, typename>
class JSONFilePersister;

template <typename... TS>
class JSONFilePersister<TypeList<TS...>, NoCustomPersisterParam> {
 public:
  using variant_t = Variant<TS...>;
  using transaction_t = std::vector<variant_t>;  // Mock to make it compile.
  using fields_update_function_t = std::function<void(const variant_t&)>;

  explicit JSONFilePersister(std::mutex&, fields_update_function_t f, const std::string& filename)
      : filename_(filename) {
    Replay(f);
  }

  PersisterDataAuthority DataAuthority() const { return PersisterDataAuthority::Own; }

  void PersistJournal(MutationJournal& journal) {
    if (!journal.commit_log.empty()) {
      std::ofstream os(filename_, std::fstream::app);
      if (!os.good()) {
        throw StorageCannotAppendToFileException(filename_);  // LCOV_EXCL_LINE
      }
      for (auto&& entry : journal.commit_log) {
        os << JSON(variant_t(std::move(entry))) << '\n';
      }
    }
    journal.Clear();
  }

  void InternalExposeStream() {}  // No-op to make it compile.

 private:
  std::string filename_;

  template <typename F>
  void Replay(F&& f) {
    std::ifstream is(filename_);
    if (is.good()) {
      for (std::string line; std::getline(is, line);) {
        f(std::move(ParseJSON<variant_t>(line)));
      }
    }
  }
};

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::JSONFilePersister;

#endif  // CURRENT_STORAGE_PERSISTER_FILE_H
