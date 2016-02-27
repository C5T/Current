/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

// A simple, reference, implementation of an file-based persister.
// The file is replayed at startup to check its integriry and to extract the most recent index/timestamp.
// Each iterator opens the same file again, to read its first N lines.
// Uses `ScopeOwnedBy{Me,SomeoneElse}`.

#ifndef BLOCKS_PERSISTENCE_FILE_H
#define BLOCKS_PERSISTENCE_FILE_H

#include <atomic>
#include <functional>
#include <fstream>

#include "exceptions.h"

#include "../SS/persister.h"

#include "../../TypeSystem/Serialization/json.h"

#include "../../Bricks/time/chrono.h"
#include "../../Bricks/sync/scope_owned.h"
#include "../../Bricks/util/atomic_that_works.h"

namespace current {
namespace persistence {

namespace impl {
// An iterator to read a file line by line, extracting tab-separated `idxts_t index` and `const char* data`.
// Validates the entries come in the right order of 0-based indexes, and with strictly increasing timestamps.
template <typename ENTRY>
class IteratorOverFileOfPersistedEntries {
 public:
  explicit IteratorOverFileOfPersistedEntries(std::istream& fi) : fi_(fi) { assert(fi_.good()); }

  template <typename F>
  bool ProcessNextEntry(F&& f) {
    if (std::getline(fi_, line_)) {
      const size_t tab_pos = line_.find('\t');
      if (tab_pos == std::string::npos) {
        CURRENT_THROW(MalformedEntryException(line_));
      }
      const auto current = ParseJSON<idxts_t>(line_.substr(0, tab_pos));
      if (current.index != next_.index) {
        // Indexes must be strictly continuous.
        CURRENT_THROW(InconsistentIndexException(next_.index, current.index));
      }
      if (current.us < next_.us) {
        // Timestamps must monotonically increase.
        CURRENT_THROW(InconsistentTimestampException(next_.us, current.us));
      }
      f(current, line_.c_str() + tab_pos + 1);
      next_ = current;
      ++next_.index;
      ++next_.us;
      return true;
    } else {
      return false;
    }
  }

  // Return the absolute lowest possible next entry to scan or publish.
  idxts_t Next() const { return next_; }

 private:
  std::istream& fi_;
  std::string line_;
  idxts_t next_;
};

// The implementation of a persister based exclusively on appending to and reading one text flie.
template <typename ENTRY>
class FilePersister {
 protected:
  // { last_published_index + 1, last_published_us + 1us }, or { 0, 0us } for an empty persister.
  struct end_t {
    uint64_t index;
    std::chrono::microseconds us;
  };
  static_assert(sizeof(std::chrono::microseconds) == 8, "");
  static_assert(sizeof(end_t) == 16, "");

 private:
  struct Impl {
    const std::string filename;
    // std::atomic<end_t> end;
    current::atomic_that_works<end_t> end;
    std::ofstream appender;

    Impl() = delete;
    explicit Impl(const std::string& filename)
        : filename(filename),
          end(ValidateFileAndInitializeNext(filename)),
          appender(filename, std::ofstream::app) {
      assert(appender.good());
    }

    // Replay the file but ignore its contents. Used to initialize `end` at startup.
    static end_t ValidateFileAndInitializeNext(const std::string& filename) {
      std::ifstream fi(filename);
      if (fi.good()) {
        IteratorOverFileOfPersistedEntries<ENTRY> cit(fi);
        while (cit.ProcessNextEntry([](const idxts_t&, const char*) {})) {
          // Read through all the lines.
          // Let `IteratorOverFileOfPersistedEntries` maintain its own `next_`, which later becomes `this->end`.
          ;
        }
        const auto& end = cit.Next();
        return end_t{end.index, end.us};
      } else {
        return end_t{0ull, std::chrono::microseconds(0)};
      }
    }
  };

 public:
  FilePersister(const std::string& filename) : impl_(filename) {}

  class IterableRange {
   public:
    explicit IterableRange(ScopeOwnedByMe<Impl>& impl, uint64_t begin, uint64_t end)
        : impl_(impl, [this]() {}), begin_(begin), end_(end) {}

    struct Entry {
      idxts_t idx_ts;
      ENTRY entry;
    };

    class Iterator {
     public:
      explicit Iterator(const std::string& filename, uint64_t i) : i_(i) {
        if (!filename.empty()) {
          fi_ = std::make_unique<std::ifstream>(filename);
          cit_ = std::make_unique<IteratorOverFileOfPersistedEntries<ENTRY>>(*fi_);
        }
      }

      // `operator*` relies each entry will be requested at most once. -- D.K.
      Entry operator*() const {
        Entry result;
        bool found = false;
        while (!found) {
          if (!(cit_->ProcessNextEntry([this, &found, &result](const idxts_t& cursor, const char* json) {
                if (cursor.index == i_) {
                  found = true;
                  result.idx_ts = cursor;
                  result.entry = ParseJSON<ENTRY>(json);
                } else if (cursor.index > i_) {
                  CURRENT_THROW(InconsistentIndexException(i_, cursor.index));
                }
              }))) {
            CURRENT_THROW(current::Exception());  // End of file. Should never happen.
          }
        }
        return result;
      }

      void operator++() { ++i_; }
      bool operator==(const Iterator& rhs) const { return i_ == rhs.i_; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

     private:
      std::unique_ptr<std::ifstream> fi_;
      std::unique_ptr<IteratorOverFileOfPersistedEntries<ENTRY>> cit_;
      uint64_t i_;
    };

    Iterator begin() const {
      if (begin_ == end_) {
        return Iterator("", 0);
      } else {
        return Iterator(impl_->filename, begin_);
      }
    }
    Iterator end() const {
      if (begin_ == end_) {
        return Iterator("", 0);
      } else {
        return Iterator(impl_->filename, end_);
      }
    }

   private:
    mutable ScopeOwnedBySomeoneElse<Impl> impl_;
    const uint64_t begin_;
    const uint64_t end_;
  };

  template <typename E>
  idxts_t DoPublish(E&& entry, const std::chrono::microseconds timestamp) {
    end_t iterator = impl_->end.load();
    if (timestamp < iterator.us) {
      CURRENT_THROW(InconsistentTimestampException(iterator.us, timestamp));
    }
    iterator.us = timestamp;
    const auto current = idxts_t(iterator.index, iterator.us);
    impl_->appender << JSON(current) << '\t' << JSON(std::forward<E>(entry)) << std::endl;
    ++iterator.index;
    iterator.us += std::chrono::microseconds(1);
    impl_->end.store(iterator);
    return current;
  }

  bool Empty() const noexcept { return !impl_->end.load().index; }
  uint64_t Size() const noexcept { return impl_->end.load().index; }

  idxts_t LastPublishedIndexAndTimestamp() const {
    const auto iterator = impl_->end.load();
    if (iterator.index) {
      return idxts_t(iterator.index - 1, iterator.us - std::chrono::microseconds(1));
    } else {
      throw current::Exception("`LastPublishedIndexAndTimestamp()` called with no entries published.");
    }
  }

  IterableRange Iterate(uint64_t begin_index, uint64_t end_index) const {
    const uint64_t current_size = impl_->end.load().index;
    if (end_index == static_cast<uint64_t>(-1)) {
      end_index = current_size;
    }
    if (end_index > current_size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (begin_index == end_index) {
      return IterableRange(impl_, 0, 0);  // OK, even for an empty persister, where 0 is an invalid index.
    }
    if (end_index < begin_index) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    return IterableRange(impl_, begin_index, end_index);
  }

 private:
  mutable ScopeOwnedByMe<Impl> impl_;
};

}  // namespace current::persistence::impl

template <typename ENTRY>
using File = ss::EntryPersister<impl::FilePersister<ENTRY>, ENTRY>;

}  // namespace current::persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_FILE_H
