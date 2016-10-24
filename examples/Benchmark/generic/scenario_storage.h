/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BENCHMARK_SCENARIO_STORAGE_H
#define BENCHMARK_SCENARIO_STORAGE_H

#include "../../../port.h"

#include "benchmark.h"

#include "../../../Bricks/util/random.h"
#include "../../../Storage/persister/sherlock.h"
#include "../../../Storage/storage.h"

#include "../../../Bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_uint32(storage_initial_size, 10000, "The number of records initially in the storage.");
DEFINE_string(storage_transaction, "empty", "The transaction to run in the inner loop of the load test.");
DEFINE_bool(storage_test_string, false, "Set to `true` to test 'get' and 'put' with string, not int, keys.");
#else
DECLARE_uint32(storage_initial_size);
DECLARE_string(storage_transaction);
DECLARE_bool(storage_test_string);
#endif

CURRENT_STRUCT(UInt32KeyValuePair) {
  CURRENT_FIELD(key, uint32_t);
  CURRENT_FIELD(value, uint32_t);
  CURRENT_CONSTRUCTOR(UInt32KeyValuePair)(uint32_t key = 0, uint32_t value = 0) : key(key), value(value) {}
};

CURRENT_STRUCT(StringKeyValuePair) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(value, uint32_t);
  CURRENT_CONSTRUCTOR(StringKeyValuePair)(const std::string& key = "", uint32_t value = 0) : key(key), value(value) {}
};

CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, UInt32KeyValuePair, PersistedUInt32KeyValuePair);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, StringKeyValuePair, PersistedStringKeyValuePair);
CURRENT_STORAGE(KeyValueDB) {
  CURRENT_STORAGE_FIELD(hashmap_uint32, PersistedUInt32KeyValuePair);
  CURRENT_STORAGE_FIELD(hashmap_string, PersistedStringKeyValuePair);
};

SCENARIO(storage, "Storage transactions test.") {
  using storage_t = KeyValueDB<SherlockInMemoryStreamPersister>;
  storage_t db;
  size_t actual_size_uint32;
  size_t actual_size_string;
  std::function<void()> f;

  static uint32_t RandomUInt32() { return current::random::RandomIntegral<uint32_t>(1000000, 999999); }
  static std::string RandomString() { return current::ToString(RandomUInt32()); }

  storage() {
    const bool testing_string = FLAGS_storage_test_string;

    std::map<std::string, std::function<void()>> tests = {
        {{"empty"}, [this]() { db.ReadOnlyTransaction([](ImmutableFields<storage_t>) {}).Wait(); }},
        {{"size"},
         [this]() {
           db.ReadOnlyTransaction([this](ImmutableFields<storage_t> fields) {
               if (fields.hashmap_uint32.Size() != actual_size_uint32 ||
                   fields.hashmap_string.Size() != actual_size_string) {
                 std::cerr << "Test failed." << std::endl;
                 std::exit(-1);
               }
             }).Wait();
         }},
        {{"get"},
         [this, testing_string]() {
           Value(db.ReadOnlyTransaction([this, testing_string](ImmutableFields<storage_t> fields) {
                     if (!testing_string) {
                       return Exists(fields.hashmap_uint32[RandomUInt32()]);
                     } else {
                       return Exists(fields.hashmap_string[RandomString()]);
                     }
                   }).Go());
         }},
        {{"put"}, [this, testing_string]() {
           db.ReadWriteTransaction([this, testing_string](MutableFields<storage_t> fields) {
               if (!testing_string) {
                 fields.hashmap_uint32.Add(UInt32KeyValuePair(RandomUInt32(), RandomUInt32()));
               } else {
                 fields.hashmap_string.Add(StringKeyValuePair(RandomString(), RandomUInt32()));
               }
             }).Wait();
         }}};
    const auto cit = tests.find(FLAGS_storage_transaction);

    if (cit != tests.end()) {
      f = cit->second;
    } else {
      std::vector<std::string> valid;
      for (const auto& t : tests) {
        valid.push_back("'" + t.first + "'");
      }
      std::cerr << "The `--storage_transaction` flag must be " << current::strings::Join(valid, ", or ") << '.'
                << std::endl;
      CURRENT_ASSERT(false);
    }

    const auto pair = Value(db.ReadWriteTransaction([](MutableFields<storage_t> fields) {
                                for (uint32_t i = 0; i < FLAGS_storage_initial_size; ++i) {
                                  fields.hashmap_uint32.Add(UInt32KeyValuePair(RandomUInt32(), RandomUInt32()));
                                  fields.hashmap_string.Add(StringKeyValuePair(RandomString(), RandomUInt32()));
                                }
                                return std::make_pair(fields.hashmap_uint32.Size(), fields.hashmap_string.Size());
                              }).Go());
    actual_size_uint32 = pair.first;
    actual_size_string = pair.second;

    CURRENT_ASSERT(actual_size_uint32 <= FLAGS_storage_initial_size);
    if (FLAGS_storage_initial_size > 0u) {
      CURRENT_ASSERT(actual_size_uint32 > 0u);
    }
    CURRENT_ASSERT(actual_size_string <= FLAGS_storage_initial_size);
    if (FLAGS_storage_initial_size > 0u) {
      CURRENT_ASSERT(actual_size_string > 0u);
    }
  }

  void RunOneQuery() override { f(); }
};

REGISTER_SCENARIO(storage);

#endif  // BENCHMARK_SCENARIO_STORAGE_H
