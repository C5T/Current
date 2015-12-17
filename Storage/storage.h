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

// Current-friendly container types.
//
// * Vector<T> <=> std::vector<T>
//   Empty(), Size(), operator[](i), PushBack(x), PopBack() [, iteration].
//
// * OrderedDictionary<T> <=> std::map<T_KEY, T>
//   Empty(), Size(), operator[](key), Erase(key) [, iteration, {lower/upper}_bound].
//   `T_KEY` is either the type of `T.key` or of `T.get_key()`.
//
// * LightweightMatrix<T> <=> { T_ROW, T_COL } -> T, three `std::map<>`-s.
//   Empty(), Size(), Rows()/Cols(), Add(cell), Delete(row, col) [, iteration, {lower/upper}_bound].
//   `T_ROW` and `T_COL` are either the type of `T.row` / `T.col`, or of `T.get_row()` / `T.get_col()`.
//
// * TODO(dkorolev): UnorderedDictionary.
// ...
//
// All Current-friendly types support persistence.
//
// Only allow default constructors for containers.

#ifndef CURRENT_STORAGE_STORAGE_H
#define CURRENT_STORAGE_STORAGE_H

#include "../port.h"

#include <fstream>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "base.h"

#include "container/dictionary.h"
#include "container/vector.h"

#include "persister/file.h"

#include "../TypeSystem/struct.h"
#include "../TypeSystem/Serialization/json.h"
#include "../TypeSystem/optional.h"

#include "../Bricks/exception.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/strings/strings.h"

#include "../Bricks/cerealize/cerealize.h"  // TODO(dkorolev): Deprecate.

namespace current {
namespace storage {

// `CURRENT_STORAGE_STRUCT_ALIAS`:
// 1) Creates a dedicated C++ type to allow compile-time disambiguation of storages of same underlying types.
// 2) Splits the type into `T_ADDER` and `T_DELETER`, to support seamless persistence of deletions.
// clang-format off

#ifndef _MSC_VER

#define CURRENT_STORAGE_STRUCT_ALIAS(base, alias)                                            \
  CURRENT_STRUCT(CURRENT_STORAGE_ADDER_##alias, base) {                                      \
    CURRENT_DEFAULT_CONSTRUCTOR(CURRENT_STORAGE_ADDER_##alias) {}                            \
    CURRENT_CONSTRUCTOR( CURRENT_STORAGE_ADDER_##alias)(const base& value) : base(value) {}  \
  };                                                                                         \
  CURRENT_STRUCT(CURRENT_STORAGE_DELETER_##alias, base) {                                    \
    CURRENT_DEFAULT_CONSTRUCTOR(CURRENT_STORAGE_DELETER_##alias) {}                          \
    CURRENT_CONSTRUCTOR(CURRENT_STORAGE_DELETER_##alias)(const base& value) : base(value) {} \
  };                                                                                         \
  struct alias {                                                                             \
    using T_ENTRY = base;                                                                    \
    using T_ADDER = CURRENT_STORAGE_ADDER_##alias;                                           \
    using T_DELETER = CURRENT_STORAGE_DELETER_##alias;                                       \
  }

#else  // _MSC_VER

// The MSVS version uses `SUPER` instead of `base` in the initializer list.
#define CURRENT_STORAGE_STRUCT_ALIAS(base, alias)                                             \
  CURRENT_STRUCT(CURRENT_STORAGE_ADDER_##alias, base) {                                       \
    CURRENT_DEFAULT_CONSTRUCTOR(CURRENT_STORAGE_ADDER_##alias) {}                             \
    CURRENT_CONSTRUCTOR( CURRENT_STORAGE_ADDER_##alias)(const base& value) : SUPER(value) {}  \
  };                                                                                          \
  CURRENT_STRUCT(CURRENT_STORAGE_DELETER_##alias, base) {                                     \
    CURRENT_DEFAULT_CONSTRUCTOR(CURRENT_STORAGE_DELETER_##alias) {}                           \
    CURRENT_CONSTRUCTOR(CURRENT_STORAGE_DELETER_##alias)(const base& value) : SUPER(value) {} \
  };                                                                                          \
  struct alias {                                                                              \
    using T_ENTRY = base;                                                                     \
    using T_ADDER = CURRENT_STORAGE_ADDER_##alias;                                            \
    using T_DELETER = CURRENT_STORAGE_DELETER_##alias;                                        \
  }

#endif  // _MSC_VER

// clang-format on

#define CURRENT_STORAGE_FIELDS_HELPERS(name)                                                                   \
  template <typename T>                                                                                        \
  struct CURRENT_STORAGE_FIELDS_HELPER;                                                                        \
  template <>                                                                                                  \
  struct CURRENT_STORAGE_FIELDS_HELPER<CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>> {     \
    constexpr static size_t CURRENT_STORAGE_FIELD_INDEX_BASE = __COUNTER__;                                    \
    typedef CURRENT_STORAGE_FIELDS_##name<::current::storage::CountFields> CURRENT_STORAGE_FIELD_COUNT_STRUCT; \
  }

// clang-format off
#define CURRENT_STORAGE_IMPLEMENTATION(name)                                                                  \
  template <typename INSTANTIATION_TYPE>                                                                      \
  struct CURRENT_STORAGE_FIELDS_##name;                                                                       \
  template <template <typename...> class PERSISTER, typename FIELDS>                                          \
  struct CURRENT_STORAGE_IMPL_##name : FIELDS {                                                               \
   private:                                                                                                   \
    constexpr static size_t fields_count = ::current::storage::FieldCounter<FIELDS>::value;                   \
    using T_FIELDS_TYPE_LIST = ::current::storage::FieldsTypeList<FIELDS, fields_count>;                      \
    using T_FIELDS_VARIANT = Variant<T_FIELDS_TYPE_LIST>;                                                     \
    PERSISTER<T_FIELDS_TYPE_LIST> persister_;                                                                 \
    std::mutex mutex_;                                                                                        \
                                                                                                              \
   public:                                                                                                    \
    using T_FIELDS_BY_REFERENCE = FIELDS&;                                                                    \
    CURRENT_STORAGE_IMPL_##name() = delete;                                                                   \
    CURRENT_STORAGE_IMPL_##name& operator=(const CURRENT_STORAGE_IMPL_##name&) = delete;                      \
    template <typename... ARGS>                                                                               \
    CURRENT_STORAGE_IMPL_##name(ARGS&&... args) : persister_(std::forward<ARGS>(args)...) {                   \
      persister_.Replay([this](T_FIELDS_VARIANT && entry) { entry.Call(*this); });                            \
    }                                                                                                         \
    template <typename F>                                                                                     \
    void Transaction(F&& f) {                                                                                 \
      std::lock_guard<std::mutex> lock(mutex_);                                                               \
      FIELDS::current_storage_mutation_journal_.AssertEmpty();                                                \
      bool successful = false;                                                                                \
      try {                                                                                                   \
        f(static_cast<FIELDS&>(*this));                                                                       \
        successful = true;                                                                                    \
      } catch (std::exception&) {                                                                             \
        FIELDS::current_storage_mutation_journal_.Rollback();                                                 \
      }                                                                                                       \
      if (successful) {                                                                                       \
        persister_.PersistJournal(FIELDS::current_storage_mutation_journal_);                                 \
      }                                                                                                       \
    }                                                                                                         \
    template <typename F1, typename F2>                                                                       \
    void Transaction(F1&& f1, F2&& f2) {                                                                      \
      std::lock_guard<std::mutex> lock(mutex_);                                                               \
      FIELDS::current_storage_mutation_journal_.AssertEmpty();                                                \
      bool successful = false;                                                                                \
      try {                                                                                                   \
        f2(f1(static_cast<FIELDS&>(*this)));                                                                  \
        successful = true;                                                                                    \
      } catch (std::exception&) {                                                                             \
        FIELDS::current_storage_mutation_journal_.Rollback();                                                 \
      }                                                                                                       \
      if (successful) {                                                                                       \
        persister_.PersistJournal(FIELDS::current_storage_mutation_journal_);                                 \
      }                                                                                                       \
    }                                                                                                         \
    size_t FieldsCount() const { return fields_count; }                                                       \
  };                                                                                                          \
  template <template <typename...> class PERSISTER>                                                           \
  using name = CURRENT_STORAGE_IMPL_##name<PERSISTER,                                                         \
                                           CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>>; \
  CURRENT_STORAGE_FIELDS_HELPERS(name)
// clang-format on

#define CURRENT_STORAGE(name)            \
  CURRENT_STORAGE_IMPLEMENTATION(name);  \
  template <typename INSTANTIATION_TYPE> \
  struct CURRENT_STORAGE_FIELDS_##name   \
      : ::current::storage::FieldsBase<  \
            CURRENT_STORAGE_FIELDS_HELPER<CURRENT_STORAGE_FIELDS_##name<::current::storage::DeclareFields>>>

#define CURRENT_STORAGE_FIELD(field_name, field_type, item_alias, ...)                                     \
  ::current::storage::FieldInfo<item_alias::T_ADDER, item_alias::T_DELETER> operator()(                    \
      ::current::storage::Index<CURRENT_EXPAND_MACRO(__COUNTER__) - CURRENT_STORAGE_FIELD_INDEX_BASE - 1>) \
      const {                                                                                              \
    return ::current::storage::FieldInfo<item_alias::T_ADDER, item_alias::T_DELETER>();                    \
  }                                                                                                        \
  using T_FIELD_TYPE_##field_name = ::current::storage::Field<                                             \
      INSTANTIATION_TYPE,                                                                                  \
      field_type<item_alias::T_ENTRY, item_alias::T_ADDER, item_alias::T_DELETER, ##__VA_ARGS__>>;         \
  T_FIELD_TYPE_##field_name field_name = T_FIELD_TYPE_##field_name(current_storage_mutation_journal_);     \
  void operator()(const item_alias::T_ADDER& adder) { field_name(adder); }                                 \
  void operator()(const item_alias::T_DELETER& deleter) { field_name(deleter); }

template <typename STORAGE>
using CurrentStorage = typename STORAGE::T_FIELDS_BY_REFERENCE;

#if 0
struct CannotPopBackFromEmptyVectorException : Exception {};
typedef const CannotPopBackFromEmptyVectorException& CannotPopBackFromEmptyVector;

template <typename T>
class VectorStorage {
 public:
  std::vector<T> vector_;
};

template <typename T, typename PERSISTER>
class VectorAPI : protected VectorStorage<T> {
 public:
  explicit VectorAPI(PERSISTER* persister) : persister_(*persister) {}

  bool Empty() const { return VectorStorage<T>::vector_.empty(); }
  size_t Size() const { return VectorStorage<T>::vector_.size(); }

  typename PERSISTER::ImmutableOptionalType operator[](size_t index) const {
    if (index < VectorStorage<T>::vector_.size()) {
      return typename PERSISTER::ImmutableOptionalType(FromBarePointer(), &VectorStorage<T>::vector_[index]);
    } else {
      return typename PERSISTER::ImmutableOptionalType(nullptr);
    }
  }

  void PushBack(const T& object) {
    persister_.PersistPushBack(VectorStorage<T>::vector_.size(), object);
    VectorStorage<T>::vector_.push_back(object);
  }

  void PopBack() {
    if (!VectorStorage<T>::vector_.empty()) {
      persister_.PersistPopBack(VectorStorage<T>::vector_.size());
      VectorStorage<T>::vector_.pop_back();
    } else {
      CURRENT_THROW(CannotPopBackFromEmptyVectorException());
    }
  }

 private:
  PERSISTER& persister_;
};

template <typename T>
class OrderedDictionaryStorage {
 public:
  using T_KEY = sfinae::ENTRY_KEY_TYPE<T>;
  using T_MAP = std::map<T_KEY, T>;
  T_MAP map_;
  void DoInsert(const T& object) { map_[sfinae::GetKey(object)] = object; }
};

template <typename T, typename PERSISTER>
class OrderedDictionaryAPI : protected OrderedDictionaryStorage<T> {
 public:
  typedef typename OrderedDictionaryStorage<T>::T_MAP T_MAP;
  typedef typename OrderedDictionaryStorage<T>::T_KEY T_KEY;
  explicit OrderedDictionaryAPI(PERSISTER* persister) : persister_(*persister) {}

  bool Empty() const { return OrderedDictionaryStorage<T>::map_.empty(); }
  size_t Size() const { return OrderedDictionaryStorage<T>::map_.size(); }

  typename PERSISTER::ImmutableOptionalType operator[](sfinae::CF<T_KEY> key) const {
    const auto iterator = OrderedDictionaryStorage<T>::map_.find(key);
    if (iterator != OrderedDictionaryStorage<T>::map_.end()) {
      return typename PERSISTER::ImmutableOptionalType(FromBarePointer(), &iterator->second);
    } else {
      return nullptr;
    }
  }

  void Insert(const T& object) {
    persister_.PersistInsert(object);
    OrderedDictionaryStorage<T>::DoInsert(object);
  }

  void Erase(sfinae::CF<T_KEY> key) {
    persister_.PersistErase(key);
    OrderedDictionaryStorage<T>::map_.erase(key);
  }

  struct Iterator final {
    using T_ITERATOR = typename T_MAP::const_iterator;
    T_ITERATOR iterator;
    explicit Iterator(T_ITERATOR iterator) : iterator(std::move(iterator)) {}
    void operator++() { ++iterator; }
    bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    sfinae::CF<T_KEY> key() const { return iterator->first; }
    const T& operator*() const { return iterator->second; }
    const T* operator->() const { return &iterator->second; }
  };

  Iterator begin() const { return Iterator(OrderedDictionaryStorage<T>::map_.cbegin()); }
  Iterator end() const { return Iterator(OrderedDictionaryStorage<T>::map_.cend()); }

 private:
  PERSISTER& persister_;
};

template <typename T>
class LightweightMatrixStorage {
 public:
  using T_ROW = sfinae::ENTRY_ROW_TYPE<T>;
  using T_COL = sfinae::ENTRY_COL_TYPE<T>;
  std::map<std::pair<T_ROW, T_COL>, std::unique_ptr<T>> map_;
  std::map<T_ROW, std::map<T_COL, const T*>> forward_;
  std::map<T_COL, std::map<T_ROW, const T*>> transposed_;

  void DoAdd(const T& object) {
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    auto& placeholder = map_[std::make_pair(row, col)];
    placeholder = make_unique<T>(object);
    forward_[row][col] = placeholder.get();
    transposed_[col][row] = placeholder.get();
  }

  void DoDelete(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) {
    forward_[row].erase(col);
    transposed_[col].erase(row);
    if (forward_[row].empty()) {
      forward_.erase(row);
    }
    if (transposed_[col].empty()) {
      transposed_.erase(col);
    }
    map_.erase(std::make_pair(row, col));
  }
};

template <typename T, typename PERSISTER>
class LightweightMatrixAPI : protected LightweightMatrixStorage<T> {
 public:
  typedef typename LightweightMatrixStorage<T>::T_ROW T_ROW;
  typedef typename LightweightMatrixStorage<T>::T_COL T_COL;
  explicit LightweightMatrixAPI(PERSISTER* persister) : persister_(*persister) {}

  bool Empty() const { return LightweightMatrixStorage<T>::map_.empty(); }
  size_t Size() const { return LightweightMatrixStorage<T>::map_.size(); }

  template <typename OUTER, typename INNER>
  struct InnerAccessor final {
    using T_MAP = std::map<INNER, const T*>;
    const sfinae::CF<OUTER> key_;
    const T_MAP& map_;

    struct Iterator final {
      using T_ITERATOR = typename T_MAP::const_iterator;
      T_ITERATOR iterator;
      explicit Iterator(T_ITERATOR iterator) : iterator(iterator) {}
      void operator++() { ++iterator; }
      bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      sfinae::CF<INNER> key() const { return iterator->first; }
      const T& operator*() const { return *iterator->second; }
      const T* operator->() const { return iterator->second; }
    };

    InnerAccessor(sfinae::CF<OUTER> key, const T_MAP& map) : key_(key), map_(map) {}

    bool Empty() const { return map_.empty(); }
    size_t Size() const { return map_.size(); }

    sfinae::CF<OUTER> Key() const { return key_; }

    bool Has(const INNER& x) const { return map_.find(x) != map_.end(); }

    Iterator begin() const { return Iterator(map_.cbegin()); }
    Iterator end() const { return Iterator(map_.cend()); }
  };

  template <typename OUTER, typename INNER>
  struct OuterAccessor final {
    using T_MAP = std::map<OUTER, std::map<INNER, const T*>>;
    const T_MAP& map_;

    struct OuterIterator final {
      using T_ITERATOR = typename T_MAP::const_iterator;
      T_ITERATOR iterator;
      explicit OuterIterator(T_ITERATOR iterator) : iterator(iterator) {}
      void operator++() { ++iterator; }
      bool operator==(const OuterIterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const OuterIterator& rhs) const { return !operator==(rhs); }
      sfinae::CF<OUTER> key() const { return iterator->first; }
      InnerAccessor<OUTER, INNER> operator*() const {
        return InnerAccessor<OUTER, INNER>(iterator->first, iterator->second);
      }
    };

    explicit OuterAccessor(const T_MAP& map) : map_(map) {}

    bool Empty() const { return map_.empty(); }

    size_t Size() const { return map_.size(); }

    bool Has(const OUTER& x) const { return map_.find(x) != map_.end(); }

    ImmutableOptional<InnerAccessor<OUTER, INNER>> operator[](sfinae::CF<OUTER> key) const {
      const auto iterator = map_.find(key);
      if (iterator != map_.end()) {
        return std::move(make_unique<InnerAccessor<OUTER, INNER>>(key, iterator->second));
      } else {
        return nullptr;
      }
    }

    OuterIterator begin() const { return OuterIterator(map_.cbegin()); }
    OuterIterator end() const { return OuterIterator(map_.cend()); }
  };

  OuterAccessor<T_ROW, T_COL> Rows() const {
    return OuterAccessor<T_ROW, T_COL>(LightweightMatrixStorage<T>::forward_);
  }

  OuterAccessor<T_COL, T_ROW> Cols() const {
    return OuterAccessor<T_COL, T_ROW>(LightweightMatrixStorage<T>::transposed_);
  }

  void Add(const T& object) {
    persister_.PersistAdd(object);
    LightweightMatrixStorage<T>::DoAdd(object);
  }

  void Delete(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) {
    persister_.PersistDelete(row, col);
    LightweightMatrixStorage<T>::DoDelete(row, col);
  }

  bool Has(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) const {
    return LightweightMatrixStorage<T>::map_.find(std::make_pair(row, col)) !=
           LightweightMatrixStorage<T>::map_.end();
  }

  ImmutableOptional<T> Get(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) const {
    const auto result = LightweightMatrixStorage<T>::map_.find(std::make_pair(row, col));
    if (result != LightweightMatrixStorage<T>::map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), result->second.get());
    } else {
      return nullptr;
    }
  }

 private:
  PERSISTER& persister_;
};

struct InMemory final {
  class Instance final {
   public:
    void Run() {}
  };

  template <typename T>
  class VectorPersister {
   public:
    using ImmutableOptionalType = ImmutableOptional<T>;

    VectorPersister(const std::string&, Instance&, VectorStorage<T>&) {}

    void PersistPushBack(size_t, const T&) {}
    void PersistPopBack(size_t) {}
  };

  template <typename T>
  class OrderedDictionaryPersister {
   public:
    using T_KEY = sfinae::ENTRY_KEY_TYPE<T>;

    using ImmutableOptionalType = ImmutableOptional<T>;

    OrderedDictionaryPersister(const std::string&, Instance&, OrderedDictionaryStorage<T>&) {}

    void PersistInsert(const T&) {}
    void PersistErase(sfinae::CF<T_KEY>) {}
  };

  template <typename T>
  class LightweightMatrixPersister {
   public:
    using T_ROW = sfinae::ENTRY_ROW_TYPE<T>;
    using T_COL = sfinae::ENTRY_COL_TYPE<T>;

    using ImmutableOptionalType = ImmutableOptional<T>;

    LightweightMatrixPersister(const std::string&, Instance&, LightweightMatrixStorage<T>&) {}

    void PersistAdd(const T&) {}
    void PersistDelete(const T_ROW&, const T_COL&) {}
  };
};

struct ReplayFromAndAppendToFile final {
  class Instance final {
   public:
    explicit Instance(const std::string& filename) : filename_(filename) {}
    void Run() {
      assert(!run_);
      run_ = true;
      assert(!output_file_);

      {
        std::ifstream fi(filename_);
        for (std::string line; std::getline(fi, line);) {
          // Format: TIMESTAMP + '\t' + HOOK_NAME '\t' + USER DATA.
          const char* s = line.c_str();
          const char* e = s + line.length();
          const char* tab_between_timestamp_and_hook_name = std::find(s, e, '\t');
          assert(*tab_between_timestamp_and_hook_name);
          ++tab_between_timestamp_and_hook_name;
          const char* tab_between_hook_name_and_data = std::find(tab_between_timestamp_and_hook_name, e, '\t');
          assert(*tab_between_hook_name_and_data);
          const auto hook =
              hooks_.find(std::string(tab_between_timestamp_and_hook_name, tab_between_hook_name_and_data));
          ++tab_between_hook_name_and_data;
          assert(hook != hooks_.end());
          hook->second(tab_between_hook_name_and_data);
        }
      }

      output_file_.reset(new std::ofstream(filename_, std::fstream::app));
      assert(output_file_->good());
    }

    void RegisterHook(const std::string& hook_name, std::function<void(const char*)> hook) {
      assert(hook);
      auto& placeholder = hooks_[hook_name];
      assert(!placeholder);
      placeholder = hook;
    }

    std::ostream& Persist(const std::string& hook_name) {
      assert(output_file_);
      return (*output_file_) << current::time::Now().count() << '\t' << hook_name << '\t';
    }

   private:
    bool run_ = false;
    const std::string filename_;
    std::unique_ptr<std::ofstream> output_file_;
    std::map<std::string, std::function<void(const char*)>> hooks_;
  };

  template <typename T>
  class VectorPersister {
   public:
    using ImmutableOptionalType = ImmutableOptional<T>;

    VectorPersister(const std::string& name, Instance& instance, VectorStorage<T>& storage)
        : instance_(instance),
          hook_push_back_name_(name + ".push_back"),
          hook_pop_back_name_(name + ".pop_back") {
      instance.RegisterHook(hook_push_back_name_,
                            [&storage](const char* data) {
                              const auto index = current::strings::FromString<size_t>(data);
                              assert(index == storage.vector_.size());
                              const char* tab = std::find(data, data + strlen(data), '\t');
                              assert(*tab);
                              storage.vector_.push_back(ParseJSON<T>(tab + 1));
                            });
      instance.RegisterHook(hook_pop_back_name_,
                            [&storage](const char* data) {
                              const auto index = current::strings::FromString<size_t>(data);
                              assert(index == storage.vector_.size());
                              assert(!storage.vector_.empty());
                              storage.vector_.pop_back();
                            });
    }

    void PersistPushBack(size_t i, const T& x) {
      instance_.Persist(hook_push_back_name_) << i << '\t' << JSON(x) << '\n' << std::flush;
    }
    void PersistPopBack(size_t i) { instance_.Persist(hook_pop_back_name_) << i << '\n' << std::flush; }

   private:
    Instance& instance_;
    const std::string hook_push_back_name_;
    const std::string hook_pop_back_name_;
  };

  template <typename T>
  class OrderedDictionaryPersister {
   public:
    using T_KEY = sfinae::ENTRY_KEY_TYPE<T>;

    using ImmutableOptionalType = ImmutableOptional<T>;

    OrderedDictionaryPersister(const std::string& name,
                               Instance& instance,
                               OrderedDictionaryStorage<T>& storage)
        : instance_(instance), hook_insert_name_(name + ".insert"), hook_erase_name_(name + ".erase") {
      instance.RegisterHook(hook_insert_name_,
                            [&storage](const char* data) { storage.DoInsert(ParseJSON<T>(data)); });
      instance.RegisterHook(hook_erase_name_,
                            [&storage](const char* data) { storage.map_.erase(ParseJSON<T_KEY>(data)); });
    }

    void PersistInsert(const T& x) { instance_.Persist(hook_insert_name_) << JSON(x) << '\n' << std::flush; }

    void PersistErase(const T_KEY& k) { instance_.Persist(hook_erase_name_) << JSON(k) << '\n' << std::flush; }

   private:
    Instance& instance_;
    const std::string hook_insert_name_;
    const std::string hook_erase_name_;
  };

  template <typename T>
  class LightweightMatrixPersister {
   public:
    using T_ROW = sfinae::ENTRY_ROW_TYPE<T>;
    using T_COL = sfinae::ENTRY_COL_TYPE<T>;

    using ImmutableOptionalType = ImmutableOptional<T>;

    LightweightMatrixPersister(const std::string& name,
                               Instance& instance,
                               LightweightMatrixStorage<T>& storage)
        : instance_(instance), hook_add_name_(name + ".add"), hook_delete_name_(name + ".delete") {
      instance.RegisterHook(hook_add_name_,
                            [&storage](const char* data) { storage.DoAdd(ParseJSON<T>(data)); });
      instance.RegisterHook(hook_delete_name_,
                            [&storage](const char* data) {
                              const std::vector<std::string> fields = current::strings::Split(data, '\t');
                              assert(fields.size() == 2);
                              storage.DoDelete(ParseJSON<T_ROW>(fields[0]), ParseJSON<T_COL>(fields[1]));
                            });
    }

    void PersistAdd(const T& x) { instance_.Persist(hook_add_name_) << JSON(x) << '\n' << std::flush; }
    void PersistDelete(const T_ROW& row, const T_COL& col) {
      instance_.Persist(hook_delete_name_) << JSON(row) << '\t' << JSON(col) << '\n' << std::flush;
    }

   private:
    Instance& instance_;
    const std::string hook_add_name_;
    const std::string hook_delete_name_;
  };
};

// TODO(dkorolev): Deprecate.
struct ReplayFromAndAppendToFileUsingCereal final {
  class Instance final {
   public:
    explicit Instance(const std::string& filename) : filename_(filename) {}
    void Run() {
      assert(!run_);
      run_ = true;
      assert(!output_file_);

      {
        std::ifstream fi(filename_);
        for (std::string line; std::getline(fi, line);) {
          // Format: TIMESTAMP + '\t' + HOOK_NAME '\t' + USER DATA.
          const char* s = line.c_str();
          const char* e = s + line.length();
          const char* tab_between_timestamp_and_hook_name = std::find(s, e, '\t');
          assert(*tab_between_timestamp_and_hook_name);
          ++tab_between_timestamp_and_hook_name;
          const char* tab_between_hook_name_and_data = std::find(tab_between_timestamp_and_hook_name, e, '\t');
          assert(*tab_between_hook_name_and_data);
          const auto hook =
              hooks_.find(std::string(tab_between_timestamp_and_hook_name, tab_between_hook_name_and_data));
          ++tab_between_hook_name_and_data;
          assert(hook != hooks_.end());
          hook->second(tab_between_hook_name_and_data);
        }
      }

      output_file_.reset(new std::ofstream(filename_, std::fstream::app));
      assert(output_file_->good());
    }

    void RegisterHook(const std::string& hook_name, std::function<void(const char*)> hook) {
      assert(hook);
      auto& placeholder = hooks_[hook_name];
      assert(!placeholder);
      placeholder = hook;
    }

    std::ostream& Persist(const std::string& hook_name) {
      assert(output_file_);
      return (*output_file_) << current::time::Now().count() << '\t' << hook_name << '\t';
    }

   private:
    bool run_ = false;
    const std::string filename_;
    std::unique_ptr<std::ofstream> output_file_;
    std::map<std::string, std::function<void(const char*)>> hooks_;
  };

  template <typename T>
  class VectorPersister {
   public:
    using ImmutableOptionalType = ImmutableOptional<T>;

    VectorPersister(const std::string& name, Instance& instance, VectorStorage<T>& storage)
        : instance_(instance),
          hook_push_back_name_(name + ".push_back"),
          hook_pop_back_name_(name + ".pop_back") {
      instance.RegisterHook(hook_push_back_name_,
                            [&storage](const char* data) {
                              const auto index = current::strings::FromString<size_t>(data);
                              assert(index == storage.vector_.size());
                              const char* tab = std::find(data, data + strlen(data), '\t');
                              assert(*tab);
                              storage.vector_.push_back(CerealizeParseJSON<T>(tab + 1));
                            });
      instance.RegisterHook(hook_pop_back_name_,
                            [&storage](const char* data) {
                              const auto index = current::strings::FromString<size_t>(data);
                              assert(index == storage.vector_.size());
                              assert(!storage.vector_.empty());
                              storage.vector_.pop_back();
                            });
    }

    void PersistPushBack(size_t i, const T& x) {
      instance_.Persist(hook_push_back_name_) << i << '\t' << CerealizeJSON(x) << '\n' << std::flush;
    }
    void PersistPopBack(size_t i) { instance_.Persist(hook_pop_back_name_) << i << '\n' << std::flush; }

   private:
    Instance& instance_;
    const std::string hook_push_back_name_;
    const std::string hook_pop_back_name_;
  };

  template <typename T>
  class OrderedDictionaryPersister {
   public:
    using T_KEY = sfinae::ENTRY_KEY_TYPE<T>;

    using ImmutableOptionalType = ImmutableOptional<T>;

    OrderedDictionaryPersister(const std::string& name,
                               Instance& instance,
                               OrderedDictionaryStorage<T>& storage)
        : instance_(instance), hook_insert_name_(name + ".insert"), hook_erase_name_(name + ".erase") {
      instance.RegisterHook(hook_insert_name_,
                            [&storage](const char* data) { storage.DoInsert(CerealizeParseJSON<T>(data)); });
      instance.RegisterHook(
          hook_erase_name_,
          [&storage](const char* data) { storage.map_.erase(CerealizeParseJSON<T_KEY>(data)); });
    }

    void PersistInsert(const T& x) {
      instance_.Persist(hook_insert_name_) << CerealizeJSON(x) << '\n' << std::flush;
    }

    void PersistErase(const T_KEY& k) {
      instance_.Persist(hook_erase_name_) << CerealizeJSON(k) << '\n' << std::flush;
    }

   private:
    Instance& instance_;
    const std::string hook_insert_name_;
    const std::string hook_erase_name_;
  };

  template <typename T>
  class LightweightMatrixPersister {
   public:
    using T_ROW = sfinae::ENTRY_ROW_TYPE<T>;
    using T_COL = sfinae::ENTRY_COL_TYPE<T>;

    using ImmutableOptionalType = ImmutableOptional<T>;

    LightweightMatrixPersister(const std::string& name,
                               Instance& instance,
                               LightweightMatrixStorage<T>& storage)
        : instance_(instance), hook_add_name_(name + ".add"), hook_delete_name_(name + ".delete") {
      instance.RegisterHook(hook_add_name_,
                            [&storage](const char* data) { storage.DoAdd(CerealizeParseJSON<T>(data)); });
      instance.RegisterHook(hook_delete_name_,
                            [&storage](const char* data) {
                              const std::vector<std::string> fields = current::strings::Split(data, '\t');
                              assert(fields.size() == 2);
                              storage.DoDelete(CerealizeParseJSON<T_ROW>(fields[0]),
                                               CerealizeParseJSON<T_COL>(fields[1]));
                            });
    }

    void PersistAdd(const T& x) { instance_.Persist(hook_add_name_) << CerealizeJSON(x) << '\n' << std::flush; }
    void PersistDelete(const T_ROW& row, const T_COL& col) {
      instance_.Persist(hook_delete_name_) << CerealizeJSON(row) << '\t' << CerealizeJSON(col) << '\n'
                                           << std::flush;
    }

   private:
    Instance& instance_;
    const std::string hook_add_name_;
    const std::string hook_delete_name_;
  };
};

template <typename T, class POLICY = InMemory>
class Vector final : public VectorAPI<T, typename POLICY::template VectorPersister<T>>,
                     protected POLICY::template VectorPersister<T> {
 public:
  template <typename... ARGS>
  Vector(const std::string& name, typename POLICY::Instance& instance)
      : VectorAPI<T, typename POLICY::template VectorPersister<T>>(this),
        POLICY::template VectorPersister<T>(name, instance, *this) {}
};

template <typename T, class POLICY = InMemory>
class OrderedDictionary final
    : public OrderedDictionaryAPI<T, typename POLICY::template OrderedDictionaryPersister<T>>,
      protected POLICY::template OrderedDictionaryPersister<T> {
 public:
  template <typename... ARGS>
  OrderedDictionary(const std::string& name, typename POLICY::Instance& instance)
      : OrderedDictionaryAPI<T, typename POLICY::template OrderedDictionaryPersister<T>>(this),
        POLICY::template OrderedDictionaryPersister<T>(name, instance, *this) {}
};

template <typename T, class POLICY = InMemory>
class LightweightMatrix final
    : public LightweightMatrixAPI<T, typename POLICY::template LightweightMatrixPersister<T>>,
      protected POLICY::template LightweightMatrixPersister<T> {
 public:
  template <typename... ARGS>
  LightweightMatrix(const std::string& name, typename POLICY::Instance& instance)
      : LightweightMatrixAPI<T, typename POLICY::template LightweightMatrixPersister<T>>(this),
        POLICY::template LightweightMatrixPersister<T>(name, instance, *this) {}
};
#endif
}  // namespace current::storage

}  // namespace current

using current::storage::CurrentStorage;

#endif  // CURRENT_STORAGE_STORAGE_H
