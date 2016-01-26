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
#include "../rest/hypermedia.h"
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

  const auto rest1 = RESTfulStorage<TestStorage>(storage, FLAGS_client_storage_test_port, "/api1");
  const auto rest2 = RESTfulStorage<TestStorage, current::storage::rest::Hypermedia>(
      storage,
      FLAGS_client_storage_test_port,
      "/api2");

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_client_storage_test_port);

  // Top-level.
  {
    // Not exposed by default.
    const auto result = HTTP(GET(base_url + "/api1"));
    EXPECT_EQ(404, static_cast<int>(result.code));
  }
  {
    // Exposed by `Hypermedia`.
    {
      const auto result = HTTP(GET(base_url + "/api2"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ(base_url + "/healthz", ParseJSON<HypermediaRESTTopLevel>(result.body).url_healthz);
    }
    {
      const auto result = HTTP(GET(base_url + "/api2/healthz"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_TRUE(ParseJSON<HypermediaRESTHealthz>(result.body).up);
    }
  }

  // GET a non-existing resource.
  {
    const auto result = HTTP(GET(base_url + "/api1/client/42"));
    EXPECT_EQ(404, static_cast<int>(result.code));
    EXPECT_EQ("Nope.\n", result.body);
  }

  {
    const auto result = HTTP(GET(base_url + "/api2/client/42"));
    EXPECT_EQ(404, static_cast<int>(result.code));
    EXPECT_EQ("{\"error\":\"Resource not found.\"}\n", result.body);
  }

  // POST to a full resource-specifying URL, not allowed.
  {
    const auto result = HTTP(POST(base_url + "/api1/client/42", "blah"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("Should not have resource key in the URL.\n", result.body);
  }

  {
    const auto result = HTTP(POST(base_url + "/api2/client/42", "blah"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("{\"error\":\"Should not have resource key in the URL.\"}\n", result.body);
  }

  // POST a JSON not following the schema, not allowed.
  {
    const auto result = HTTP(POST(base_url + "/api1/client", "{\"trash\":true}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("Bad JSON.\n", result.body);
  }

  {
    const auto result = HTTP(POST(base_url + "/api2/client", "{\"trash\":true}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ(
      "{"
      "\"error\":\"Invalid JSON in request body.\","
      "\"json_details\":\"Expected number for `key`, got: missing field.\""
      "}\n",
      result.body);
  }

  // POST another JSON not following the schema, still not allowed.
  {
    const auto result = HTTP(POST(base_url + "/api1/client", "{\"key\":[]}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("Bad JSON.\n", result.body);
  }

  {
    const auto result = HTTP(POST(base_url + "/api2/client", "{\"key\":[]}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ(
      "{"
      "\"error\":\"Invalid JSON in request body.\","
      "\"json_details\":\"Expected number for `key`, got: []\""
      "}\n",
      result.body);
  }

  // POST a real piece.
  EXPECT_EQ(201, static_cast<int>(HTTP(POST(base_url + "/api1/client", Client(ClientID(42)))).code));

  // Now GET it via both APIs.
  {
    const auto result = HTTP(GET(base_url + "/api1/client/42"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"key\":42,\"name\":\"John Doe\",\"white\":true,\"straight\":true,\"male\":true}\n", result.body);
  }

  {
    const auto result = HTTP(GET(base_url + "/api2/client/42"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"key\":42,\"name\":\"John Doe\",\"white\":true,\"straight\":true,\"male\":true}\n", result.body);
  }

  // PUT an entry with the key different from URL is not allowed.
  EXPECT_EQ(400, static_cast<int>(HTTP(PUT(base_url + "/api1/client/42", Client(ClientID(64)))).code));
  EXPECT_EQ(400, static_cast<int>(HTTP(PUT(base_url + "/api2/client/42", Client(ClientID(64)))).code));

  // PUT a modified entry via both APIs.
  Client updated_client_42(ClientID(42));
  updated_client_42.name = "Jane Doe";
  EXPECT_EQ(200, static_cast<int>(HTTP(PUT(base_url + "/api1/client/42", updated_client_42)).code));
  updated_client_42.male = false;
  EXPECT_EQ(200, static_cast<int>(HTTP(PUT(base_url + "/api2/client/42", updated_client_42)).code));

  // Check if both updates took place.
  {
    const auto result = HTTP(GET(base_url + "/api1/client/42"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"key\":42,\"name\":\"Jane Doe\",\"white\":true,\"straight\":true,\"male\":false}\n", result.body);
  }
}
