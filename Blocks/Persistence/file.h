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
// Iterators never outlive the persister.

#ifndef BLOCKS_PERSISTENCE_FILE_H
#define BLOCKS_PERSISTENCE_FILE_H

#include <atomic>
#include <fstream>
#include <functional>

#include "exceptions.h"

#include "../SS/persister.h"
#include "../SS/signature.h"

#include "../../Bricks/sync/locks.h"
#include "../../Bricks/sync/scope_owned.h"
#include "../../Bricks/time/chrono.h"
#include "../../Bricks/util/atomic_that_works.h"
#include "../../TypeSystem/Schema/schema.h"
#include "../../TypeSystem/Serialization/json.h"

namespace current {
namespace persistence {

namespace impl {

namespace constants {
constexpr const char kDirectiveMarker = '#';
constexpr const char* kSignatureDirective = "#signature";
constexpr const char* kHeadDirective = "#head";
constexpr const char* kHeadFromatString = "%020lld";
}  // namespace current::persistence::impl::constants

typedef int64_t head_value_t;

// An iterator to read a file line by line, extracting tab-separated `idxts_t index` and `const char* data`.
// Validates the entries come in the right order of 0-based indexes, and with strictly increasing timestamps.
template <typename ENTRY>
class IteratorOverFileOfPersistedEntries {
 public:
  explicit IteratorOverFileOfPersistedEntries(std::istream& fi, std::streampos offset, uint64_t index_at_offset)
      : fi_(fi), next_(index_at_offset, std::chrono::microseconds(0)) {
    CURRENT_ASSERT(!fi_.bad());
    if (offset) {
      fi_.seekg(offset, std::ios_base::beg);
    }
  }

  template <typename F1, typename F2>
  bool ProcessNextEntry(F1&& on_entry, F2&& on_directive) {
    if (std::getline(fi_, line_)) {
      // A directive always starts with kDirectiveMarker ('#'),
      // an entry - with JSON-serialized `idxts_t` object
      if (line_[0] != constants::kDirectiveMarker) {
        const size_t tab_pos = line_.find('\t');
        if (tab_pos == std::string::npos) {
          CURRENT_THROW(MalformedEntryException(line_));
        }
        const auto current = ParseJSON<idxts_t>(line_.substr(0, tab_pos));
        if (current.index != next_.index) {
          // Indexes must be strictly continuous.
          CURRENT_THROW(ss::InconsistentIndexException(next_.index, current.index));
        }
        if (current.us < next_.us) {
          // Timestamps must monotonically increase.
          CURRENT_THROW(ss::InconsistentTimestampException(next_.us, current.us));
        }
        on_entry(current, line_.c_str() + tab_pos + 1);
        next_ = current;
        ++next_.index;
        ++next_.us;
      } else {
        on_directive(line_);
      }
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
  // { last_published_index + 1, last_published_us, current_head_us }, or { 0, -1us, -1us } for an empty persister.
  struct end_t {
    uint64_t next_index;
    std::chrono::microseconds last_entry_us;
    std::chrono::microseconds head;
  };
  static_assert(sizeof(std::chrono::microseconds) == 8, "");

 private:
  struct FilePersisterImpl final {
    const std::string filename;
    std::ofstream appender;
    std::fstream head_rewriter;

    // `offset.size() == end.next_index`, and `offset[i]` is the offset in bytes where the line for index `i` begins.
    std::mutex& mutex_ref;  // Guards `offset`, `head_offset` and `timestamp`.
    std::vector<std::streampos> offset;
    std::streamoff head_offset;
    std::vector<std::chrono::microseconds> timestamp;

    // Just `std::atomic<end_t> end;` won't work in g++ until 5.1, ref.
    // http://stackoverflow.com/questions/29824570/segfault-in-stdatomic-load/29824840#29824840
    // std::atomic<end_t> end;
    current::atomic_that_works<end_t> end;

    FilePersisterImpl() = delete;
    FilePersisterImpl(const FilePersisterImpl&) = delete;
    FilePersisterImpl(FilePersisterImpl&&) = delete;
    FilePersisterImpl& operator=(const FilePersisterImpl&) = delete;
    FilePersisterImpl& operator=(FilePersisterImpl&&) = delete;

    explicit FilePersisterImpl(std::mutex& mutex_ref,
                               const ss::StreamNamespaceName& namespace_name,
                               const std::string& filename)
        : filename(filename),
          appender(filename, std::ofstream::app),
          head_rewriter(filename, std::ofstream::in | std::ofstream::out),
          mutex_ref(mutex_ref),
          head_offset(0) {
      ValidateFileAndInitializeHead(namespace_name);
      if (appender.bad() || head_rewriter.bad()) {
        CURRENT_THROW(PersistenceFileNotWritable(filename));
      }
    }

    // Replay the file but ignore its contents. Used to initialize `end` at startup.
    void ValidateFileAndInitializeHead(const ss::StreamNamespaceName& namespace_name) {
      std::ifstream fi(filename);
      if (!fi.bad()) {
        // Read through all the lines.
        // Let `IteratorOverFileOfPersistedEntries` maintain its own `next_`, which later becomes `this->end`.
        // While reading the file, record the offset of each record and store it in `offset`.
        IteratorOverFileOfPersistedEntries<ENTRY> cit(fi, 0, 0);
        std::streampos current_offset(0);
        auto head = std::chrono::microseconds(-1);
        reflection::StructSchema struct_schema;
        struct_schema.AddType<ENTRY>();
        const auto signature = JSON(ss::StreamSignature(namespace_name, struct_schema.GetSchemaInfo()));
        while (cit.ProcessNextEntry(
            [&](const idxts_t& current, const char*) {
              CURRENT_ASSERT(current.index == offset.size());
              CURRENT_ASSERT(current.index == timestamp.size());
              if (!(current.us > head)) {
                CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), current.us));
              }
              offset.push_back(current_offset);
              timestamp.push_back(current.us);
              current_offset = fi.tellg();
              head = current.us;
              head_offset = 0;
            },
            [&](const std::string& value) {
              static const auto head_key_length = strlen(constants::kHeadDirective);
              static const auto signature_key_length = strlen(constants::kSignatureDirective);
              head_offset = 0;
              if (!value.compare(0, head_key_length, constants::kHeadDirective)) {
                auto offset = head_key_length;
                while (std::isspace(value[offset])) {
                  ++offset;
                }
                const auto us = std::chrono::microseconds(current::FromString<head_value_t>(value.c_str() + offset));
                if (!(us > head)) {
                  CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), us));
                }
                head = us;
                head_offset = std::streamoff(current_offset) + offset;
              } else if (!value.compare(0, signature_key_length, constants::kSignatureDirective)) {
                // The signature, if present, should be at the beginning of the file.
                if (current_offset != 0) {
                  CURRENT_THROW(InvalidSignatureLocation());
                }
                auto offset = signature_key_length;
                while (std::isspace(value[offset])) {
                  ++offset;
                }
                if (value.compare(offset, signature.length(), signature)) {
                  CURRENT_THROW(InvalidStreamSignature(signature, value.substr(offset)));
                }
              }
              current_offset = fi.tellg();
            })) {
          ;
        }
        const auto& next = cit.Next();
        // The `next.us` stores the closest possible next entry timestamp,
        // so the last processed entry timestamp is always 1us less.
        end.store({next.index, next.us - std::chrono::microseconds(1), head});
        // Append the signature if there is neither entries nor directives in the file.
        if (!current_offset) {
          appender << constants::kSignatureDirective << ' ' << signature << std::endl;
        }
      } else {
        end.store({0ull, std::chrono::microseconds(-1), std::chrono::microseconds(-1)});
      }
    }
  };

 public:
  FilePersister() = delete;
  FilePersister(const FilePersister&) = delete;
  FilePersister(FilePersister&&) = delete;
  FilePersister& operator=(const FilePersister&) = delete;
  FilePersister& operator=(FilePersister&&) = delete;

  explicit FilePersister(std::mutex& mutex_ref,
                         const ss::StreamNamespaceName& namespace_name,
                         const std::string& filename)
      : file_persister_impl_(mutex_ref, namespace_name, filename) {}

  class IterableRange {
   public:
    explicit IterableRange(ScopeOwned<FilePersisterImpl>& file_persister_impl,
                           uint64_t begin,
                           uint64_t end,
                           std::streampos begin_offset)
        : file_persister_impl_(file_persister_impl, [this]() { valid_ = false; }),
          begin_(begin),
          end_(end),
          begin_offset_(begin_offset) {}

    struct Entry {
      idxts_t idx_ts;
      ENTRY entry;
    };

    class Iterator final {
     public:
      Iterator() = delete;
      Iterator(const Iterator&) = delete;
      Iterator(Iterator&&) = default;
      Iterator& operator=(const Iterator&) = delete;
      Iterator& operator=(Iterator&&) = default;

      Iterator(ScopeOwned<FilePersisterImpl>& file_persister_impl,
               const std::string& filename,
               uint64_t i,
               std::streampos offset,
               uint64_t index_at_offset)
          : file_persister_impl_(file_persister_impl, [this]() { valid_ = false; }), i_(i) {
        if (!filename.empty()) {
          fi_ = std::make_unique<std::ifstream>(filename);
          // This `if` condition is only here to test performance with vs. without the `seekg`.
          // The high performance version jumps to the desired entry right away,
          // The poor performance one scans the file from the very beginning for each new iterator created.
          if (true) {
            cit_ = std::make_unique<IteratorOverFileOfPersistedEntries<ENTRY>>(*fi_, offset, index_at_offset);
          } else {
            // Inefficient, scan the file from the very beginning.
            cit_ = std::make_unique<IteratorOverFileOfPersistedEntries<ENTRY>>(*fi_, 0, 0);
          }
        }
      }

      // `operator*` relies on the fact each entry will be requested at most once.
      // The range-based for-loop works fine. -- D.K.
      Entry operator*() const {
        if (!valid_) {
          CURRENT_THROW(PersistenceFileNoLongerAvailable(
              file_persister_impl_.ObjectAccessorDespitePossiblyDestructing().filename));
        }
        Entry result;
        bool found = false;
        while (!found) {
          if (!(cit_->ProcessNextEntry(
                  [this, &found, &result](const idxts_t& cursor, const char* json) {
                    if (cursor.index == i_) {
                      found = true;
                      result.idx_ts = cursor;
                      result.entry = ParseJSON<ENTRY>(json);
                    } else if (cursor.index > i_) {                                     // LCOV_EXCL_LINE
                      CURRENT_THROW(ss::InconsistentIndexException(i_, cursor.index));  // LCOV_EXCL_LINE
                    }
                  },
                  [](const std::string&) {}))) {
            // End of file. Should never happen as long as the user only iterates over valid ranges.
            CURRENT_THROW(current::Exception());  // LCOV_EXCL_LINE
          }
        }
        return result;
      }

      void operator++() {
        if (!valid_) {
          CURRENT_THROW(PersistenceFileNoLongerAvailable(
              file_persister_impl_.ObjectAccessorDespitePossiblyDestructing().filename));
        }
        ++i_;
      }
      bool operator==(const Iterator& rhs) const { return i_ == rhs.i_; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      operator bool() const { return valid_; }

     private:
      ScopeOwnedBySomeoneElse<FilePersisterImpl> file_persister_impl_;
      bool valid_ = true;
      std::unique_ptr<std::ifstream> fi_;
      std::unique_ptr<IteratorOverFileOfPersistedEntries<ENTRY>> cit_;
      uint64_t i_;
    };

    Iterator begin() const {
      if (!valid_) {
        CURRENT_THROW(
            PersistenceFileNoLongerAvailable(file_persister_impl_.ObjectAccessorDespitePossiblyDestructing().filename));
      }
      if (begin_ == end_) {
        return Iterator(file_persister_impl_, "", 0, 0, 0);  // No need in accessing the file for a null iterator.
      } else {
        return Iterator(file_persister_impl_, file_persister_impl_->filename, begin_, begin_offset_, begin_);
      }
    }
    Iterator end() const {
      if (!valid_) {
        CURRENT_THROW(
            PersistenceFileNoLongerAvailable(file_persister_impl_.ObjectAccessorDespitePossiblyDestructing().filename));
      }
      if (begin_ == end_) {
        return Iterator(file_persister_impl_, "", 0, 0, 0);  // No need in accessing the file for a null iterator.
      } else {
        return Iterator(
            file_persister_impl_, "", end_, 0, 0);  // No need in accessing the file for a no-op `end` iterator.
      }
    }

    operator bool() const { return valid_; }

   private:
    mutable ScopeOwnedBySomeoneElse<FilePersisterImpl> file_persister_impl_;
    bool valid_ = true;
    const uint64_t begin_;
    const uint64_t end_;
    const std::streampos begin_offset_;
  };

  template <current::locks::MutexLockStatus MLS, typename E>
  idxts_t DoPublish(E&& entry, const std::chrono::microseconds timestamp) {
    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->mutex_ref);

    end_t iterator = file_persister_impl_->end.load();
    if (!(timestamp > iterator.head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(iterator.head + std::chrono::microseconds(1), timestamp));
    }

    iterator.last_entry_us = iterator.head = timestamp;
    const auto current = idxts_t(iterator.next_index, iterator.last_entry_us);
    CURRENT_ASSERT(file_persister_impl_->offset.size() == iterator.next_index);
    CURRENT_ASSERT(file_persister_impl_->timestamp.size() == iterator.next_index);
    file_persister_impl_->offset.push_back(file_persister_impl_->appender.tellp());
    file_persister_impl_->timestamp.push_back(timestamp);

    file_persister_impl_->appender << JSON(current) << '\t' << JSON(std::forward<E>(entry)) << std::endl;
    ++iterator.next_index;
    file_persister_impl_->head_offset = 0;
    file_persister_impl_->end.store(iterator);

    return current;
  }

  template <current::locks::MutexLockStatus MLS>
  void DoUpdateHead(const std::chrono::microseconds timestamp) {
    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->mutex_ref);

    end_t iterator = file_persister_impl_->end.load();
    if (!(timestamp > iterator.head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(iterator.head + std::chrono::microseconds(1), timestamp));
    }
    iterator.head = timestamp;
    const auto head_str = Printf(constants::kHeadFromatString, timestamp.count());
    if (file_persister_impl_->head_offset) {
      auto& rewriter = file_persister_impl_->head_rewriter;
      rewriter.seekp(file_persister_impl_->head_offset, std::ios_base::beg);
      rewriter << head_str << std::endl;
    } else {
      auto& appender = file_persister_impl_->appender;
      appender << constants::kHeadDirective << ' ';
      file_persister_impl_->head_offset = appender.tellp();
      appender << head_str << std::endl;
    }
    file_persister_impl_->end.store(iterator);
  }

  template <current::locks::MutexLockStatus>
  bool Empty() const noexcept {
    return !file_persister_impl_->end.load().next_index;
  }
  template <current::locks::MutexLockStatus>
  uint64_t Size() const noexcept {
    return file_persister_impl_->end.load().next_index;
  }

  idxts_t LastPublishedIndexAndTimestamp() const {
    const auto iterator = file_persister_impl_->end.load();
    if (iterator.next_index) {
      return idxts_t(iterator.next_index - 1, iterator.last_entry_us);
    } else {
      CURRENT_THROW(NoEntriesPublishedYet());
    }
  }

  head_optidxts_t HeadAndLastPublishedIndexAndTimestamp() const noexcept {
    const auto iterator = file_persister_impl_->end.load();
    if (iterator.next_index) {
      return head_optidxts_t(iterator.head, iterator.next_index - 1, iterator.last_entry_us);
    } else {
      return head_optidxts_t(iterator.head);
    }
  }

  template <current::locks::MutexLockStatus>
  std::chrono::microseconds CurrentHead() const noexcept {
    return file_persister_impl_->end.load().head;
  }

  std::pair<uint64_t, uint64_t> IndexRangeByTimestampRange(std::chrono::microseconds from,
                                                           std::chrono::microseconds till) const {
    std::pair<uint64_t, uint64_t> result{static_cast<uint64_t>(-1), static_cast<uint64_t>(-1)};
    std::lock_guard<std::mutex> lock(file_persister_impl_->mutex_ref);
    const auto begin_it =
        std::lower_bound(file_persister_impl_->timestamp.begin(),
                         file_persister_impl_->timestamp.end(),
                         from,
                         [](std::chrono::microseconds entry_t, std::chrono::microseconds t) { return entry_t < t; });
    if (begin_it != file_persister_impl_->timestamp.end()) {
      result.first = std::distance(file_persister_impl_->timestamp.begin(), begin_it);
    }
    if (till.count() > 0) {
      const auto end_it =
          std::upper_bound(file_persister_impl_->timestamp.begin(),
                           file_persister_impl_->timestamp.end(),
                           till,
                           [](std::chrono::microseconds t, std::chrono::microseconds entry_t) { return t < entry_t; });
      if (end_it != file_persister_impl_->timestamp.end()) {
        result.second = std::distance(file_persister_impl_->timestamp.begin(), end_it);
      }
    }
    return result;
  }

  IterableRange Iterate(uint64_t begin_index, uint64_t end_index) const {
    const uint64_t current_size = file_persister_impl_->end.load().next_index;
    if (end_index == static_cast<uint64_t>(-1)) {
      end_index = current_size;
    }
    if (end_index > current_size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (begin_index == end_index) {
      return IterableRange(
          file_persister_impl_, 0, 0, 0);  // OK, even for an empty persister, where 0 is an invalid index.
    }
    if (end_index < begin_index) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    std::lock_guard<std::mutex> lock(file_persister_impl_->mutex_ref);
    CURRENT_ASSERT(file_persister_impl_->offset.size() >=
                   current_size);  // "Greater" is OK, `Iterate()` is multithreaded. -- D.K.
    return IterableRange(file_persister_impl_, begin_index, end_index, file_persister_impl_->offset[begin_index]);
  }

  IterableRange Iterate(std::chrono::microseconds from, std::chrono::microseconds till) const {
    if (till.count() > 0 && till < from) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    const auto index_range = IndexRangeByTimestampRange(from, till);
    if (index_range.first != static_cast<uint64_t>(-1)) {
      return Iterate(index_range.first, index_range.second);
    } else {  // No entries found in the given range.
      return IterableRange(file_persister_impl_, 0, 0, 0);
    }
  }

 private:
  mutable ScopeOwnedByMe<FilePersisterImpl> file_persister_impl_;
};

}  // namespace current::persistence::impl

template <typename ENTRY>
using File = ss::EntryPersister<impl::FilePersister<ENTRY>, ENTRY>;

}  // namespace current::persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_FILE_H
