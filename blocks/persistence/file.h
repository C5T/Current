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

#ifdef CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
#include <iostream>
#endif  // CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS

#include "exceptions.h"

#include "../ss/persister.h"
#include "../ss/signature.h"

#include "../../bricks/sync/locks.h"
#include "../../bricks/sync/owned_borrowed.h"
#include "../../bricks/time/chrono.h"
#include "../../bricks/util/atomic_that_works.h"
#include "../../typesystem/schema/schema.h"
#include "../../typesystem/serialization/json.h"

namespace current {
namespace persistence {

namespace impl {

namespace constants {
constexpr char kDirectiveMarker = '#';
constexpr char kSignatureDirective[] = "#signature";
constexpr char kHeadDirective[] = "#head";
constexpr char kHeadFormatString[] = "%020lld";
}  // namespace current::persistence::impl::constants

typedef int64_t head_value_t;

// An iterator to read a file line by line, extracting tab-separated `idxts_t index` and `const char* data`.
// Validates the entries come in the right order of 0-based indexes, and with strictly increasing timestamps.
template <typename ENTRY>
class IteratorOverFileOfPersistedEntries {
 public:
  IteratorOverFileOfPersistedEntries(std::istream& fi, std::streampos offset, uint64_t index_at_offset)
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

template <typename DESIRED, typename ACTUAL>
struct MakeSureTheRightTypeIsSerialized {
  template <typename T>
  static DESIRED DoIt(T&& x) {
    return DESIRED(std::forward<T>(x));
  }
};

template <typename T>
struct MakeSureTheRightTypeIsSerialized<T, T> {
  static const T& DoIt(const T& x) { return x; }
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
    const std::string filename_;
    std::ofstream file_appender_;
    std::fstream head_rewriter_;

    // `record_offset_.size() == end.next_index`,
    // and `record_offset_[i]` is the record_offset_ in bytes where the line for index `i` begins.
    std::mutex& publish_mutex_ref_;  // Guards `record_offset_`, `head_offset_` and `record_timestamp_`.
    std::vector<std::streampos> record_offset_;
    std::streampos head_offset_;
    std::vector<std::chrono::microseconds> record_timestamp_;

    // Just `std::atomic<end_t> end_;` won't work in g++ until 5.1, ref.
    // http://stackoverflow.com/questions/29824570/segfault-in-stdatomic-load/29824840#29824840
    // std::atomic<end_t> end_;
    current::atomic_that_works<end_t> end_;

    FilePersisterImpl() = delete;
    FilePersisterImpl(const FilePersisterImpl&) = delete;
    FilePersisterImpl(FilePersisterImpl&&) = delete;
    FilePersisterImpl& operator=(const FilePersisterImpl&) = delete;
    FilePersisterImpl& operator=(FilePersisterImpl&&) = delete;

    FilePersisterImpl(std::mutex& publish_mutex_ref,
                      const ss::StreamNamespaceName& namespace_name,
                      const std::string& filename)
        : filename_(filename),
          file_appender_(filename, std::ofstream::app | std::ofstream::ate),
          head_rewriter_(filename, std::ofstream::in | std::ofstream::out),
          publish_mutex_ref_(publish_mutex_ref),
          head_offset_(0) {
      ValidateFileAndInitializeHead(namespace_name);
      if (file_appender_.bad() || head_rewriter_.bad()) {
        CURRENT_THROW(PersistenceFileNotWritable(filename));
      }
    }

    // Replay the file but ignore its contents. Used to initialize `end_` at startup.
    void ValidateFileAndInitializeHead(const ss::StreamNamespaceName& namespace_name) {
      std::ifstream fi(filename_);
      if (!fi.bad()) {
        // Read through all the lines.
        // Let `IteratorOverFileOfPersistedEntries` maintain its own `next_`, which later becomes `this->end_`.
        // While reading the file, record the offset of each record and store it in `record_offset_`.
        IteratorOverFileOfPersistedEntries<ENTRY> cit(fi, 0, 0);
        const std::streampos offset_zero(0);
        auto current_offset = offset_zero;
        auto head = std::chrono::microseconds(-1);
        reflection::StructSchema struct_schema;
        struct_schema.AddType<ENTRY>();
        const auto signature = JSON(ss::StreamSignature(namespace_name, struct_schema.GetSchemaInfo()));
        while (cit.ProcessNextEntry(
            [&](const idxts_t& current, const char*) {
              CURRENT_ASSERT(current.index == record_offset_.size());
              CURRENT_ASSERT(current.index == record_timestamp_.size());
              if (!(current.us > head)) {
                CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), current.us));
              }
              record_offset_.push_back(current_offset);
              record_timestamp_.push_back(current.us);
              current_offset = fi.tellg();
              head = current.us;
              head_offset_ = 0;
            },
            [&](const std::string& value) {
              static const auto head_key_length = strlen(constants::kHeadDirective);
              static const auto signature_key_length = strlen(constants::kSignatureDirective);
              head_offset_ = 0;
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
                head_offset_ = std::streampos(static_cast<size_t>(current_offset) + offset);
              } else if (!value.compare(0, signature_key_length, constants::kSignatureDirective)) {
                // The signature, if present, should be at the beginning of the file.
                if (current_offset != offset_zero) {
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
        end_.store({next.index, next.us - std::chrono::microseconds(1), head});
        // Append the signature if there is neither entries nor directives in the file.
        if (!current_offset) {
          file_appender_ << constants::kSignatureDirective << ' ' << signature << std::endl;
        }
      } else {
        end_.store({0ull, std::chrono::microseconds(-1), std::chrono::microseconds(-1)});
      }
    }
  };

 public:
  FilePersister() = delete;
  FilePersister(const FilePersister&) = delete;
  FilePersister(FilePersister&&) = delete;
  FilePersister& operator=(const FilePersister&) = delete;
  FilePersister& operator=(FilePersister&&) = delete;

  FilePersister(std::mutex& publish_mutex_ref,
                const ss::StreamNamespaceName& namespace_name,
                const std::string& filename)
      : file_persister_impl_(MakeOwned<FilePersisterImpl>(publish_mutex_ref, namespace_name, filename)) {}

  class Iterator final {
   public:
    struct Entry {
      idxts_t idx_ts;
      ENTRY entry;
    };

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    Iterator(Iterator&&) = default;
    Iterator& operator=(Iterator&&) = default;

    Iterator(Borrowed<FilePersisterImpl> file_persister_impl,
             const std::string& filename,
             uint64_t i,
             std::streampos offset,
             uint64_t index_at_offset)
        : file_persister_impl_(std::move(file_persister_impl)), i_(i) {
      if (!filename.empty()) {
        fi_ = std::make_unique<std::ifstream>(filename);
        cit_ = std::make_unique<IteratorOverFileOfPersistedEntries<ENTRY>>(*fi_, offset, index_at_offset);
      }
    }

    // `operator*` relies on the fact each entry will be requested at most once.
    // The range-based for-loop works fine. -- D.K.
    Entry operator*() const {
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

    Iterator& operator++() {
      // By convention, iterating over data, being an immutable operation, does not throw.
      ++i_;
      return *this;
    }
    bool operator==(const Iterator& rhs) const { return i_ == rhs.i_; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    operator bool() const { return file_persister_impl_; }

   private:
    const Borrowed<FilePersisterImpl> file_persister_impl_;
    std::unique_ptr<std::ifstream> fi_;
    std::unique_ptr<IteratorOverFileOfPersistedEntries<ENTRY>> cit_;
    uint64_t i_;
  };

  class IteratorUnsafe final {
   public:
    IteratorUnsafe() = delete;
    IteratorUnsafe(const IteratorUnsafe&) = delete;
    IteratorUnsafe(IteratorUnsafe&&) = default;
    IteratorUnsafe& operator=(const IteratorUnsafe&) = delete;
    IteratorUnsafe& operator=(IteratorUnsafe&&) = default;

    IteratorUnsafe(Borrowed<FilePersisterImpl> file_persister_impl,
                   const std::string& filename,
                   uint64_t i,
                   std::streampos offset,
                   uint64_t)
        : file_persister_impl_(std::move(file_persister_impl)), i_(i), current_offset_(offset) {
      if (!filename.empty()) {
        fi_ = std::make_unique<std::ifstream>(filename);
        CURRENT_ASSERT(!fi_->bad());
        if (offset) {
          fi_->seekg(offset, std::ios_base::beg);
        }
      }
    }

    // `operator*` relies on the fact each entry will be requested at most once.
    // The range-based for-loop works fine. -- D.K.
    std::string operator*() const {
      if (current_entry_.empty()) {
        const auto offset = file_persister_impl_->record_offset_[static_cast<size_t>(i_)];
        if (offset != current_offset_) {
          fi_->seekg(offset, std::ios_base::beg);
          current_offset_ = offset;
        }
        if (std::getline(*fi_, current_entry_)) {
          CURRENT_ASSERT(current_entry_[0] != constants::kDirectiveMarker);
        } else {
          // End of file. Should never happen as long as the user only iterates over valid ranges.
          CURRENT_THROW(current::Exception());  // LCOV_EXCL_LINE
        }
      }
      return current_entry_;
    }

    IteratorUnsafe& operator++() {
      ++i_;
      current_entry_.clear();
      return *this;
    }
    bool operator==(const IteratorUnsafe& rhs) const { return i_ == rhs.i_; }
    bool operator!=(const IteratorUnsafe& rhs) const { return !operator==(rhs); }
    operator bool() const { return file_persister_impl_; }

   private:
    Borrowed<FilePersisterImpl> file_persister_impl_;
    bool valid_ = true;
    std::unique_ptr<std::ifstream> fi_;
    uint64_t i_;
    mutable std::string current_entry_;
    mutable std::streampos current_offset_;
  };

  template <typename ITERATOR>
  class IterableRangeImpl {
   public:
    IterableRangeImpl(Borrowed<FilePersisterImpl> file_persister_impl,
                      uint64_t begin,
                      uint64_t end,
                      std::streampos begin_offset)
        : file_persister_impl_(std::move(file_persister_impl)), begin_(begin), end_(end), begin_offset_(begin_offset) {}

    IterableRangeImpl(IterableRangeImpl&& rhs)
        : file_persister_impl_(std::move(rhs.file_persister_impl_)),
          begin_(rhs.begin_),
          end_(rhs.end_),
          begin_offset_(rhs.begin_offset_) {}

    ITERATOR begin() const {
      // By convention, iterating over data, being an immutable operation, does not throw.
      if (begin_ == end_) {
        return ITERATOR(file_persister_impl_, "", 0, 0, 0);  // No need in accessing the file for a null iterator.
      } else {
        return ITERATOR(file_persister_impl_, file_persister_impl_->filename_, begin_, begin_offset_, begin_);
      }
    }
    ITERATOR end() const {
      // By convention, iterating over data, being an immutable operation, does not throw.
      if (begin_ == end_) {
        return ITERATOR(file_persister_impl_, "", 0, 0, 0);  // No need in accessing the file for a null iterator.
      } else {
        return ITERATOR(
            file_persister_impl_, "", end_, 0, 0);  // No need in accessing the file for a no-op `end` iterator.
      }
    }

    operator bool() const { return file_persister_impl_; }

   private:
    const Borrowed<FilePersisterImpl> file_persister_impl_;
    const uint64_t begin_;
    const uint64_t end_;
    const std::streampos begin_offset_;
  };

  // `TIMESTAMP` can be `std::chrono::microseconds` or `current::time::DefaultTimeArgument`.
  template <current::locks::MutexLockStatus MLS, typename E, typename TIMESTAMP>
  idxts_t PersisterPublishImpl(E&& entry, const TIMESTAMP provided_timestamp) {
    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->publish_mutex_ref_);

    end_t iterator = file_persister_impl_->end_.load();
    const auto timestamp = current::time::TimestampAsMicroseconds(provided_timestamp);
    if (!(timestamp > iterator.head)) {
#ifdef CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
      std::cerr << "timestamp: " << timestamp.count() << ", iterator.head: " << iterator.head.count() << std::endl;
#endif  // CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
      CURRENT_THROW(ss::InconsistentTimestampException(iterator.head + std::chrono::microseconds(1), timestamp));
    }

    iterator.last_entry_us = iterator.head = timestamp;
    const auto idxts = idxts_t(iterator.next_index, iterator.last_entry_us);
    CURRENT_ASSERT(file_persister_impl_->record_offset_.size() == iterator.next_index);
    CURRENT_ASSERT(file_persister_impl_->record_timestamp_.size() == iterator.next_index);
    file_persister_impl_->record_offset_.push_back(file_persister_impl_->file_appender_.tellp());
    file_persister_impl_->record_timestamp_.push_back(timestamp);

    // Explicit `MakeSureTheRightTypeIsSerialized` is essential, otherwise the `Variant`'s case
    // would be serialized in an unwrapped way when passed directly.
    file_persister_impl_->file_appender_ << JSON(idxts) << '\t'
                                         << JSON(MakeSureTheRightTypeIsSerialized<ENTRY, decay_t<E>>::DoIt(
                                                std::forward<E>(entry))) << std::endl;
    ++iterator.next_index;
    file_persister_impl_->head_offset_ = 0;
    file_persister_impl_->end_.store(iterator);

    return idxts;
  }

  template <current::locks::MutexLockStatus MLS>
  idxts_t PersisterPublishUnsafeImpl(const std::string& raw_log_line) {
    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->publish_mutex_ref_);

    end_t iterator = file_persister_impl_->end_.load();
    const auto tab_pos = raw_log_line.find('\t');
    if (tab_pos == std::string::npos) {
      CURRENT_THROW(MalformedEntryException(raw_log_line));
    }
    const idxts_t idxts = ParseJSON<idxts_t>(raw_log_line.substr(0, tab_pos));
    if (idxts.index != iterator.next_index) {
      CURRENT_THROW(UnsafePublishBadIndexTimestampException(iterator.next_index, idxts.index));
    }
    if (!(idxts.us > iterator.head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(iterator.head + std::chrono::microseconds(1), idxts.us));
    }

    iterator.last_entry_us = iterator.head = idxts.us;
    CURRENT_ASSERT(file_persister_impl_->record_offset_.size() == idxts.index);
    CURRENT_ASSERT(file_persister_impl_->record_timestamp_.size() == idxts.index);
    file_persister_impl_->record_offset_.push_back(file_persister_impl_->file_appender_.tellp());
    file_persister_impl_->record_timestamp_.push_back(idxts.us);

    file_persister_impl_->file_appender_ << raw_log_line << std::endl;
    ++iterator.next_index;
    file_persister_impl_->head_offset_ = 0;
    file_persister_impl_->end_.store(iterator);

    return idxts;
  }

  template <current::locks::MutexLockStatus MLS, typename TIMESTAMP>
  void PersisterUpdateHeadImpl(const TIMESTAMP provided_timestamp) {
    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->publish_mutex_ref_);

    end_t iterator = file_persister_impl_->end_.load();
    const auto timestamp = current::time::TimestampAsMicroseconds(provided_timestamp);
    if (!(timestamp > iterator.head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(iterator.head + std::chrono::microseconds(1), timestamp));
    }
    iterator.head = timestamp;
    const auto head_str = Printf(constants::kHeadFormatString, static_cast<long long>(timestamp.count()));
    if (file_persister_impl_->head_offset_) {
      auto& rewriter = file_persister_impl_->head_rewriter_;
      rewriter.seekp(file_persister_impl_->head_offset_, std::ios_base::beg);
      rewriter << head_str << std::endl;
    } else {
      auto& file_appender_ = file_persister_impl_->file_appender_;
      file_appender_ << constants::kHeadDirective << ' ';
      file_persister_impl_->head_offset_ = file_appender_.tellp();
      file_appender_ << head_str << std::endl;
    }
    file_persister_impl_->end_.store(iterator);
  }

  template <current::locks::MutexLockStatus MLS>
  bool PersisterEmptyImpl() const {
    return !file_persister_impl_->end_.load().next_index;
  }

  template <current::locks::MutexLockStatus MLS>
  uint64_t PersisterSizeImpl() const noexcept {
    return file_persister_impl_->end_.load().next_index;
  }

  template <current::locks::MutexLockStatus MLS>
  std::chrono::microseconds PersisterCurrentHeadImpl() const noexcept {
    return file_persister_impl_->end_.load().head;
  }

  template <current::locks::MutexLockStatus MLS>
  idxts_t PersisterLastPublishedIndexAndTimestampImpl() const {
    const auto iterator = file_persister_impl_->end_.load();
    if (iterator.next_index) {
      return idxts_t(iterator.next_index - 1, iterator.last_entry_us);
    } else {
      CURRENT_THROW(NoEntriesPublishedYet());
    }
  }

  template <current::locks::MutexLockStatus MLS>
  head_optidxts_t PersisterHeadAndLastPublishedIndexAndTimestampImpl() const noexcept {
    const auto iterator = file_persister_impl_->end_.load();
    if (iterator.next_index) {
      return head_optidxts_t(iterator.head, iterator.next_index - 1, iterator.last_entry_us);
    } else {
      return head_optidxts_t(iterator.head);
    }
  }

  template <current::locks::MutexLockStatus MLS>
  std::pair<uint64_t, uint64_t> PersisterIndexRangeByTimestampRangeImpl(std::chrono::microseconds from,
                                                                        std::chrono::microseconds till) const {
    std::pair<uint64_t, uint64_t> result{static_cast<uint64_t>(-1), static_cast<uint64_t>(-1)};
    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->publish_mutex_ref_);
    const auto begin_it =
        std::lower_bound(file_persister_impl_->record_timestamp_.begin(),
                         file_persister_impl_->record_timestamp_.end(),
                         from,
                         [](std::chrono::microseconds entry_t, std::chrono::microseconds t) { return entry_t < t; });
    if (begin_it != file_persister_impl_->record_timestamp_.end()) {
      result.first = std::distance(file_persister_impl_->record_timestamp_.begin(), begin_it);
    }
    if (till.count() > 0) {
      const auto end_it =
          std::upper_bound(file_persister_impl_->record_timestamp_.begin(),
                           file_persister_impl_->record_timestamp_.end(),
                           till,
                           [](std::chrono::microseconds t, std::chrono::microseconds entry_t) { return t < entry_t; });
      if (end_it != file_persister_impl_->record_timestamp_.end()) {
        result.second = std::distance(file_persister_impl_->record_timestamp_.begin(), end_it);
      }
    }
    return result;
  }

  using IterableRange = IterableRangeImpl<Iterator>;
  using IterableRangeUnsafe = IterableRangeImpl<IteratorUnsafe>;

  template <current::locks::MutexLockStatus MLS>
  IterableRange PersisterIterate(uint64_t begin_index, uint64_t end_index) const {
    return PersisterIterateImpl<MLS, IterableRange>(begin_index, end_index);
  }

  template <current::locks::MutexLockStatus MLS>
  IterableRangeUnsafe PersisterIterateUnsafe(uint64_t begin_index, uint64_t end_index) const {
    return PersisterIterateImpl<MLS, IterableRangeUnsafe>(begin_index, end_index);
  }

  template <current::locks::MutexLockStatus MLS>
  IterableRange PersisterIterate(std::chrono::microseconds from, std::chrono::microseconds till) const {
    return PersisterIterateImpl<MLS, IterableRange>(from, till);
  }

  template <current::locks::MutexLockStatus MLS>
  IterableRangeUnsafe PersisterIterateUnsafe(std::chrono::microseconds from, std::chrono::microseconds till) const {
    return PersisterIterateImpl<MLS, IterableRangeUnsafe>(from, till);
  }

 private:
  template <current::locks::MutexLockStatus MLS, typename ITERABLE>
  ITERABLE PersisterIterateImpl(uint64_t begin_index, uint64_t end_index) const {
    // OK to only lock the mutex later, as `file_persister_impl_->end_` is an `atomic`.
    const uint64_t current_size = file_persister_impl_->end_.load().next_index;
    if (end_index == static_cast<uint64_t>(-1)) {
      end_index = current_size;
    }
    if (end_index > current_size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (begin_index == end_index) {
      return ITERABLE(file_persister_impl_, 0, 0, 0);  // OK, even for an empty persister, where 0 is an invalid index.
    }
    if (end_index < begin_index) {
      CURRENT_THROW(InvalidIterableRangeException());
    }

    current::locks::SmartMutexLockGuard<MLS> lock(file_persister_impl_->publish_mutex_ref_);

    // ">" is OK, as this call is multithreading-friendly, and more entries could have been added during this call.
    CURRENT_ASSERT(file_persister_impl_->record_offset_.size() >= current_size);

    return ITERABLE(file_persister_impl_, static_cast<size_t>(begin_index), static_cast<size_t>(end_index), file_persister_impl_->record_offset_[static_cast<size_t>(begin_index)]);
  }

  template <current::locks::MutexLockStatus MLS, typename ITERABLE>
  ITERABLE PersisterIterateImpl(std::chrono::microseconds from, std::chrono::microseconds till) const {
    if (till.count() > 0 && till < from) {
      CURRENT_THROW(InvalidIterableRangeException());
    }

    const auto index_range = PersisterIndexRangeByTimestampRangeImpl<MLS>(from, till);
    if (index_range.first != static_cast<uint64_t>(-1)) {
      return PersisterIterateImpl<MLS, ITERABLE>(index_range.first, index_range.second);
    } else {  // No entries found in the requested range.
      return ITERABLE(file_persister_impl_, 0, 0, 0);
    }
  }

 private:
  Owned<FilePersisterImpl> file_persister_impl_;  // `Owned`, as iterators borrow it.
};

}  // namespace current::persistence::impl

template <typename ENTRY>
using File = ss::EntryPersister<impl::FilePersister<ENTRY>, ENTRY>;

}  // namespace current::persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_FILE_H
