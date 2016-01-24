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

#define CURRENT_MOCK_TIME

#include "../../port.h"

#include "../storage.h"
#include "../api.h"
#include "../persister/sherlock.h"

#include "../../Blocks/HTTP/api.h"

#include "../../Bricks/dflags/dflags.h"

#include "../../3rdparty/gtest/gtest.h"

DEFINE_int32(client_storage_test_port, PickPortForUnitTest(), "");

namespace storage_docu {

CURRENT_ENUM(ClientID, uint64_t) { INVALID = 0ull };

CURRENT_STRUCT(Client) {
  CURRENT_FIELD(key, ClientID);

  CURRENT_FIELD(name, std::string, "John Doe");
  
  CURRENT_FIELD(white, bool, true);
  CURRENT_FIELD(straight, bool, true);
  CURRENT_FIELD(male, bool, true);

  CURRENT_CONSTRUCTOR(Client)(ClientID key = ClientID::INVALID) : key(key) {}
};

CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, Client, PersistedClient);

CURRENT_STORAGE(StorageOfClients) {
  CURRENT_STORAGE_FIELD(client, PersistedClient);
};

}  // namespace storage_docu

TEST(StorageDocumentation, RESTifiedStorageExample) {
  using namespace storage_docu;
  using TestStorage = StorageOfClients<SherlockInMemoryStreamPersister>;

  TestStorage storage("storage_of_clients_dummy_stream_name");

  // const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_client_storage_test_port);
  const auto rest1 = RESTfulStorage<TestStorage>(storage, FLAGS_client_storage_test_port, "/api1/");
  const auto rest2 = RESTfulStorage<TestStorage>(storage, FLAGS_client_storage_test_port, "/api2/");
}
