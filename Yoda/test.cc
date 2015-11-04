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

// Smoke test and example reference usage code.
#include "docu/docu_2_reference_code.cc"

struct YodaEntryToPersist : Padawan {
  std::string key;
  int number;
  YodaEntryToPersist(const std::string& key = "", int number = 0) : key(key), number(number) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(key), CEREAL_NVP(number));
  }
  using DeleterPersister = yoda::DictionaryGlobalDeleterPersister<std::string, __COUNTER__>;
};
CEREAL_REGISTER_TYPE(YodaEntryToPersist);
CEREAL_REGISTER_TYPE(YodaEntryToPersist::DeleterPersister);

DEFINE_string(yoda_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

const std::string yoda_golden_data =
    "{\"e\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"YodaEntryToPersist\",\"ptr_wrapper\":{"
    "\"valid\":1,\"data\":{\"key\":\"one\",\"number\":1}}}}\n"
    "{\"e\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"YodaEntryToPersist\",\"ptr_wrapper\":{"
    "\"valid\":1,\"data\":{\"key\":\"two\",\"number\":2}}}}\n";

const std::string yoda_golden_data_after_delete =
    yoda_golden_data +
    "{\"e\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"YodaEntryToPersist::DeleterPersister\",\"ptr_"
    "wrapper\":{\"valid\":1,\"data\":{\"key_to_erase\":\"one\"}}}}\n";

TEST(Yoda, WritesToFile) {
  const std::string persistence_file_name = bricks::FileSystem::JoinPath(FLAGS_yoda_test_tmpdir, "data");
  const auto persistence_file_remover = bricks::FileSystem::ScopedRmFile(persistence_file_name);

  typedef yoda::SingleFileAPI<Dictionary<YodaEntryToPersist>> PersistingAPI;
  PersistingAPI api("WritingToFileAPI", persistence_file_name);

  api.Transaction([](PersistingAPI::T_DATA data) {
    auto adder = Dictionary<YodaEntryToPersist>::Mutator(data);
    adder.Add(YodaEntryToPersist("one", 1));
    adder.Add(YodaEntryToPersist("two", 2));
  }).Wait();;
  while (bricks::FileSystem::GetFileSize(persistence_file_name) != yoda_golden_data.size()) {
    ;  // Spin lock.
  }
  EXPECT_EQ(yoda_golden_data, bricks::FileSystem::ReadFileAsString(persistence_file_name));

  api.Transaction([](PersistingAPI::T_DATA data) {
    Dictionary<YodaEntryToPersist>::Mutator(data).Delete("one");
  }).Wait();
  while (bricks::FileSystem::GetFileSize(persistence_file_name) != yoda_golden_data_after_delete.size()) {
    ;  // Spin lock.
  }
  EXPECT_EQ(yoda_golden_data_after_delete, bricks::FileSystem::ReadFileAsString(persistence_file_name));
}

TEST(Yoda, ReadsFromFile) {
  const std::string persistence_file_name = bricks::FileSystem::JoinPath(FLAGS_yoda_test_tmpdir, "data");
  const auto persistence_file_remover = bricks::FileSystem::ScopedRmFile(persistence_file_name);
  bricks::FileSystem::WriteStringToFile(yoda_golden_data, persistence_file_name.c_str());

  typedef yoda::SingleFileAPI<Dictionary<YodaEntryToPersist>> PersistingAPI;
  PersistingAPI api("ReadingFromFileAPI", persistence_file_name);

  api.Transaction([](PersistingAPI::T_DATA data) {
    const auto getter = Dictionary<YodaEntryToPersist>::Accessor(data);
    EXPECT_TRUE(getter.Has(std::string("one")));
    EXPECT_TRUE(getter.Has(std::string("two")));
    EXPECT_EQ(1, static_cast<YodaEntryToPersist>(getter.Get(std::string("one"))).number);
    EXPECT_EQ(2, static_cast<YodaEntryToPersist>(getter.Get(std::string("two"))).number);
  }).Wait();
}

TEST(Yoda, ReadsDeletionFromFile) {
  const std::string persistence_file_name = bricks::FileSystem::JoinPath(FLAGS_yoda_test_tmpdir, "data");
  const auto persistence_file_remover = bricks::FileSystem::ScopedRmFile(persistence_file_name);
  bricks::FileSystem::WriteStringToFile(yoda_golden_data_after_delete, persistence_file_name.c_str());

  typedef yoda::SingleFileAPI<Dictionary<YodaEntryToPersist>> PersistingAPI;
  PersistingAPI api("ReadingFromFileAPI", persistence_file_name);
  api.Transaction([](PersistingAPI::T_DATA data) {
    const auto getter = Dictionary<YodaEntryToPersist>::Accessor(data);
    EXPECT_FALSE(getter.Has(std::string("one")));
    EXPECT_TRUE(getter.Has(std::string("two")));
  }).Wait();
}
