/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_DOCU_DOCU_3_CODE_CC
#define CURRENT_STORAGE_DOCU_DOCU_3_CODE_CC

#define CURRENT_MOCK_TIME

#include "../../port.h"

#include "../storage.h"
#include "../api.h"
#include "../rest/plain.h"
#include "../rest/simple.h"
#include "../rest/hypermedia.h"
#include "../persister/sherlock.h"

#include "../../Blocks/HTTP/api.h"

#include "../../bricks/dflags/dflags.h"

#include "../../3rdparty/gtest/gtest.h"

#ifndef CURRENT_WINDOWS
DEFINE_string(client_storage_test_tmpdir, ".current", "Local path for the test to create temporary files in.");
#else
DEFINE_string(client_storage_test_tmpdir, "Debug", "Local path for the test to create temporary files in.");
#endif

DEFINE_int32(client_storage_test_port, PickPortForUnitTest(), "");

// Need custom indent to keep the `docu` clean. -- D.K.
// clang-format off

namespace storage_docu {

  CURRENT_ENUM(ClientID, uint64_t) { INVALID = 0ull };
  
  CURRENT_STRUCT(BriefClient) {
    CURRENT_FIELD(key, ClientID);
    CURRENT_FIELD(name, std::string, "John Doe");
    CURRENT_CONSTRUCTOR(BriefClient)(ClientID key = ClientID::INVALID) : key(key) {}

    void InitializeOwnKey() { key = static_cast<ClientID>(std::hash<std::string>()(name)); }
  };
  
  CURRENT_STRUCT(Client, BriefClient) {
    CURRENT_FIELD(white, bool, true);
    CURRENT_FIELD(straight, bool, true);
    CURRENT_FIELD(male, bool, true);

    CURRENT_CONSTRUCTOR(Client)(ClientID key = ClientID::INVALID) : SUPER(key) {}

    using brief_t = BriefClient;
  };
  
  CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, Client, PersistedClient);
  
  CURRENT_STORAGE(StorageOfClients) {
    CURRENT_STORAGE_FIELD(client, PersistedClient);
  };
  
}  // namespace storage_docu

TEST(StorageDocumentation, RESTifiedStorageExample) {
  // The `RESTifiedStorageExample` test.
  current::time::ResetToZero();
  
using namespace current::storage::rest;
using namespace storage_docu;
  using TestStorage = StorageOfClients<SherlockInMemoryStreamPersister>;
  auto storage = TestStorage::CreateMasterStorage();
  
  const auto rest1 =
      RESTfulStorage<TestStorage>(*storage, FLAGS_client_storage_test_port, "/api1", "http://example.current.ai/api1");
  const auto rest2 = RESTfulStorage<TestStorage, current::storage::rest::Simple>(
      *storage, FLAGS_client_storage_test_port, "/api2", "http://example.current.ai/api2");
  const auto rest3 = RESTfulStorage<TestStorage, current::storage::rest::Hypermedia>(
      *storage, FLAGS_client_storage_test_port, "/api3", "http://example.current.ai/api3");

  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_client_storage_test_port);
  
  // Top-level.
  {
    // Not exposed by default.
    const auto result = HTTP(GET(base_url + "/api1"));
    EXPECT_EQ(404, static_cast<int>(result.code));
  }
  {
    // Exposed by `Simple`.
    {
      const auto result = HTTP(GET(base_url + "/api2"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ("http://example.current.ai/api2/status", ParseJSON<HypermediaRESTTopLevel>(result.body).url_status);
    }
    {
      const auto result = HTTP(GET(base_url + "/api2/status"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_TRUE(ParseJSON<HypermediaRESTStatus>(result.body).up);
    }
  }
  
  // GET an empty collection.
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("", result.body);
  }
  {
    const auto result = HTTP(GET(base_url + "/api2/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":true,"
        "\"message\":null,"
        "\"error\":null,"
        "\"url\":\"http://example.current.ai/api2/data/client\","
        "\"data\":[]"
        "}\n",
        result.body);
  }
  
  // GET a non-existing resource.
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client/42"));
    EXPECT_EQ(404, static_cast<int>(result.code));
    EXPECT_EQ("Nope.\n", result.body);
  }
  
  {
    const auto result = HTTP(GET(base_url + "/api2/data/client/42"));
    EXPECT_EQ(404, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":false,"
        "\"message\":null,"
        "\"error\":"
                   "{"
                   "\"name\":\"ResourceNotFound\","
                   "\"message\":\"The requested resource was not found.\","
                   "\"details\":{\"key\":\"42\"}"
                   "}"
        "}\n",
        result.body);
  }
  
  // POST to a full resource-specifying URL, not allowed.
  {
    current::time::SetNow(std::chrono::microseconds(101));
    const auto result = HTTP(POST(base_url + "/api1/data/client/42", "blah"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("Should not have resource key in the URL.\n", result.body);
  }
  
  {
    current::time::SetNow(std::chrono::microseconds(102));
    const auto result = HTTP(POST(base_url + "/api2/data/client/42", "blah"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":false,"
        "\"message\":null,"
        "\"error\":{"
                   "\"name\":\"InvalidKey\","
                   "\"message\":\"Should not have resource key in the URL.\","
                   "\"details\":null"
                   "}"
        "}\n",
        result.body);
  }
  
  // POST a JSON not following the schema, not allowed.
  {
    current::time::SetNow(std::chrono::microseconds(103));
    const auto result = HTTP(POST(base_url + "/api1/data/client", "{\"trash\":true}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("Bad JSON.\n", result.body);
  }
  
  {
    current::time::SetNow(std::chrono::microseconds(104));
    const auto result = HTTP(POST(base_url + "/api2/data/client", "{\"trash\":true}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":false,"
        "\"message\":null,"
        "\"error\":{"
                   "\"name\":\"ParseJSONError\","
                   "\"message\":\"Invalid JSON in request body.\","
                   "\"details\":{\"error_details\":\"Expected number for `key`, got: missing field.\"}"
                   "}"
        "}\n",
        result.body);
  }
  
  // POST another JSON not following the schema, still not allowed.
  {
    current::time::SetNow(std::chrono::microseconds(105));
    const auto result = HTTP(POST(base_url + "/api1/data/client", "{\"key\":[]}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ("Bad JSON.\n", result.body);
  }
  
  {
    current::time::SetNow(std::chrono::microseconds(106));
    const auto result = HTTP(POST(base_url + "/api2/data/client", "{\"key\":[]}"));
    EXPECT_EQ(400, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":false,"
        "\"message\":null,"
        "\"error\":{"
                   "\"name\":\"ParseJSONError\","
                   "\"message\":\"Invalid JSON in request body.\","
                   "\"details\":{\"error_details\":\"Expected number for `key`, got: []\"}"
                   "}"
        "}\n",
        result.body);
  }
  
  // POST a real piece.
  current::time::SetNow(std::chrono::microseconds(107));
  const auto post_response = HTTP(POST(base_url + "/api1/data/client", Client(ClientID(42))));
  const std::string client1_key_str = post_response.body;
  const ClientID client1_key = static_cast<ClientID>(current::FromString<uint64_t>(client1_key_str));
  EXPECT_EQ(201, static_cast<int>(post_response.code));
  
  // POST the same record again, not allowed.
  {
    current::time::SetNow(std::chrono::microseconds(108));
    const auto result = HTTP(POST(base_url + "/api1/data/client", Client(ClientID(42))));
    EXPECT_EQ(409, static_cast<int>(result.code));
    EXPECT_EQ("Already exists.\n", result.body);
  }
  {
    current::time::SetNow(std::chrono::microseconds(109));
    const auto result = HTTP(POST(base_url + "/api2/data/client", Client(ClientID(42))));
    EXPECT_EQ(409, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":false,"
        "\"message\":\"Resource creation failed.\","
        "\"error\":{"
                   "\"name\":\"ResourceAlreadyExists\","
                   "\"message\":\"The resource already exists\","
                   "\"details\":{\"key\":\"" + client1_key_str + "\"}"
                   "},"
        "\"resource_url\":null"
        "}\n",
        result.body);
  }
  
  // POST the same record with the `overwrite` flag set, allowed.
  {
    current::time::SetNow(std::chrono::microseconds(110));
    const auto result = HTTP(POST(base_url + "/api1/data/client?overwrite", Client(ClientID(42))));
    EXPECT_EQ(201, static_cast<int>(result.code));
    EXPECT_EQ(client1_key_str, result.body);
  }
  {
    current::time::SetNow(std::chrono::microseconds(111));
    const auto result = HTTP(POST(base_url + "/api2/data/client?overwrite", Client(ClientID(42))));
    EXPECT_EQ(201, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":true,"
        "\"message\":\"Resource created.\","
        "\"error\":null,"
        "\"resource_url\":\"http://example.current.ai/api2/data/client/" + client1_key_str + "\""
        "}\n",
        result.body);
  }
  
  // Now GET it via both APIs.
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client/" + client1_key_str));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"key\":" + client1_key_str + ",\"name\":\"John Doe\",\"white\":true,\"straight\":true,\"male\":true}\n", result.body);
  }

  {
    const auto result = HTTP(GET(base_url + "/api2/data/client/" + client1_key_str));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":true,"
        "\"url\":\"http://example.current.ai/api2/data/client/" + client1_key_str + "\","
        "\"data\":{"
                  "\"key\":" + client1_key_str + ","
                  "\"name\":\"John Doe\","
                  "\"white\":true,"
                  "\"straight\":true,"
                  "\"male\":true"
                  "}"
        "}\n",
        result.body);
  }
  
  // PUT an entry with the key different from URL is not allowed.
  current::time::SetNow(std::chrono::microseconds(112));
  EXPECT_EQ(400, static_cast<int>(HTTP(PUT(base_url + "/api1/data/client/42", Client(ClientID(64)))).code));
  current::time::SetNow(std::chrono::microseconds(113));
  EXPECT_EQ(400, static_cast<int>(HTTP(PUT(base_url + "/api2/data/client/42", Client(ClientID(64)))).code));
  
  // PUT a modified entry via both APIs.
  Client updated_client1((ClientID(client1_key)));
  updated_client1.name = "Jane Doe";
  current::time::SetNow(std::chrono::microseconds(114));
  EXPECT_EQ(200, static_cast<int>(HTTP(PUT(base_url + "/api1/data/client/" + client1_key_str, updated_client1)).code));
  updated_client1.male = false;
  current::time::SetNow(std::chrono::microseconds(115));
  EXPECT_EQ(200, static_cast<int>(HTTP(PUT(base_url + "/api2/data/client/" + client1_key_str, updated_client1)).code));
  
  // Check if both updates took place.
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client/" + client1_key_str));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"key\":" + client1_key_str + ",\"name\":\"Jane Doe\",\"white\":true,\"straight\":true,\"male\":false}\n", result.body);
  }
  
  // GET the whole collection.
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ(client1_key_str + '\t' + JSON(updated_client1) + '\n', result.body);
  }
  
  // PUT two more records and GET the collection again.
  const auto client1 = Client(ClientID(101));
  const auto client2 = Client(ClientID(102));
  current::time::SetNow(std::chrono::microseconds(116));
  EXPECT_EQ(201, static_cast<int>(HTTP(PUT(base_url + "/api1/data/client/101", client1)).code));
  current::time::SetNow(std::chrono::microseconds(117));
  EXPECT_EQ(201, static_cast<int>(HTTP(PUT(base_url + "/api1/data/client/102", client2)).code));
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("101\t" + JSON(client1) + "\n102\t" + JSON(client2) + '\n' +
              client1_key_str + '\t' + JSON(updated_client1) + '\n',
              result.body);
  }
  {
    const auto result = HTTP(GET(base_url + "/api2/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ(
        "{"
        "\"success\":true,"
        "\"message\":null,"
        "\"error\":null,"
        "\"url\":\"http://example.current.ai/api2/data/client\","
        "\"data\":["
                  "\"http://example.current.ai/api2/data/client/101\","
                  "\"http://example.current.ai/api2/data/client/102\","
                  "\"http://example.current.ai/api2/data/client/" + client1_key_str + "\""
                  "]"
        "}\n",
        result.body);
  }
  {
    const auto result = HTTP(GET(base_url + "/api3/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    // clang-format off
    EXPECT_EQ(
        "{"
        "\"success\":true,"
        "\"url\":\"http://example.current.ai/api3/data/client?i=0&n=10\","
        "\"url_directory\":\"http://example.current.ai/api3/data/client\","
        "\"i\":0,"
        "\"n\":3,"
        "\"total\":3,"
        "\"url_next_page\":null,"
        "\"url_previous_page\":null,"
        "\"data\":["
                  "{"
                  "\"url\":\"http://example.current.ai/api3/data/client/101\","
                  "\"brief\":{"
                            "\"key\":101,"
                            "\"name\":\"John Doe\""
                            "}"
                  "},"
                  "{"
                  "\"url\":\"http://example.current.ai/api3/data/client/102\","
                  "\"brief\":{"
                            "\"key\":102,"
                            "\"name\":\"John Doe\""
                            "}"
                  "},"
                  "{"
                  "\"url\":\"http://example.current.ai/api3/data/client/" + client1_key_str + "\","
                  "\"brief\":{"
                            "\"key\":" + client1_key_str + ","
                            "\"name\":\"Jane Doe\""
                            "}"
                  "}]"
        "}\n",
        result.body);
    // clang-format on
  }
  
  // DELETE non-existing record.
  current::time::SetNow(std::chrono::microseconds(118));
  {
    const auto result = HTTP(DELETE(base_url + "/api2/data/client/100500"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"success\":true,\"message\":\"Resource didn't exist.\",\"error\":null,\"resource_url\":null}\n",
              result.body);
  }
  // DELETE one record and GET the collection again.
  current::time::SetNow(std::chrono::microseconds(119));
  {
    const auto result = HTTP(DELETE(base_url + "/api2/data/client/" + client1_key_str));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"success\":true,\"message\":\"Resource deleted.\",\"error\":null,\"resource_url\":null}\n",
              result.body);
  }
  {
    const auto result = HTTP(GET(base_url + "/api1/data/client"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("101\t" + JSON(client1) + "\n102\t" + JSON(client2) + '\n', result.body);
  }
   
}
  
TEST(StorageDocumentation, IfUnmodifiedSince) {
  // The `IfUnmodifiedSince` test.
  current::time::ResetToZero();
  
using namespace current::storage::rest;
using namespace storage_docu;
  using TestStorage = StorageOfClients<SherlockInMemoryStreamPersister>;
  auto storage = TestStorage::CreateMasterStorage();
  
  const auto basic_rest = RESTfulStorage<TestStorage>(
      *storage, FLAGS_client_storage_test_port, "/api_basic", "http://example.current.ai/api_basic");
  const auto rest = RESTfulStorage<TestStorage, current::storage::rest::Simple>(
      *storage, FLAGS_client_storage_test_port, "/api", "http://example.current.ai/api");
  
  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_client_storage_test_port);
  
  // POST one record.
  const auto publish_time_1 = current::IMFFixDateTimeStringToTimestamp("Fri, 01 Jul 2016 09:00:00 GMT");
  EXPECT_EQ(1467363600000000, publish_time_1.count());
  current::time::SetNow(publish_time_1);
  const auto post_response = HTTP(POST(base_url + "/api_basic/data/client", Client(ClientID(42))));
  const std::string client_key_str = post_response.body;
  const ClientID client_key = static_cast<ClientID>(current::FromString<uint64_t>(client_key_str));
  EXPECT_EQ(201, static_cast<int>(post_response.code));
  
  {
    const auto response = HTTP(GET(base_url + "/api/data/client/" + client_key_str));
    EXPECT_EQ(200, static_cast<int>(response.code));
    ASSERT_TRUE(response.headers.Has("Last-Modified"));
    EXPECT_EQ("Fri, 01 Jul 2016 09:00:00 GMT", response.headers.Get("Last-Modified"));
    ASSERT_TRUE(response.headers.Has("X-Current-Last-Modified"));
    EXPECT_EQ("1467363600000000", response.headers.Get("X-Current-Last-Modified"));
  }
  
  Client updated_client((ClientID(client_key)));
  updated_client.name = "Jane Doe";
  
  // Attempt to modify the record passing in the wrong value of the `X-Current-If-Unmodified-Since` header.
  {
    const auto response = HTTP(PUT(base_url + "/api/data/client/" + client_key_str, updated_client)
                                   .SetHeader("X-Current-If-Unmodified-Since", "-1"));
    EXPECT_EQ(400, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"InvalidHeader\","
                  "\"message\":\"Invalid microsecond timestamp value.\","
                  "\"details\":{"
                    "\"header\":\"X-Current-If-Unmodified-Since\","
                    "\"header_value\":\"-1\""
                  "}"
                "}"
              "}\n", response.body);
  }
  // Attempt to modify the record passing in the wrong value of the `If-Unmodified-Since` header.
  {
    const auto response = HTTP(PUT(base_url + "/api/data/client/" + client_key_str, updated_client)
                                   .SetHeader("If-Unmodified-Since", "Bad string"));
    EXPECT_EQ(400, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"InvalidHeader\","
                  "\"message\":\"Unparsable datetime value.\","
                  "\"details\":{"
                    "\"header\":\"If-Unmodified-Since\","
                    "\"header_value\":\"Bad string\""
                  "}"
                "}"
              "}\n", response.body);
  }
  
  // Attempt to modify the record with the value of the `X-Current-If-Unmodified-Since` header set to 1 us in the past.
  {
    const auto header_value = current::ToString(publish_time_1 - std::chrono::microseconds(1));
    const auto response = HTTP(PUT(base_url + "/api/data/client/" + client_key_str, updated_client)
                                   .SetHeader("X-Current-If-Unmodified-Since", header_value));
    EXPECT_EQ(412, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"ResourceWasModifiedError\","
                  "\"message\":\"Resource can not be updated as it has been modified in the meantime.\","
                  "\"details\":{"
                    "\"requested_date\":\"Fri, 01 Jul 2016 08:59:59 GMT\","
                    "\"requested_us\":\"1467363599999999\","
                    "\"resource_last_modified_date\":\"Fri, 01 Jul 2016 09:00:00 GMT\","
                    "\"resource_last_modified_us\":\"1467363600000000\""
                  "}"
                "}"
              "}\n", response.body);
  }
  // Attempt to modify the record with the value of the `If-Unmodified-Since` header set to 1 second in the past.
  {
    const auto header_value = current::FormatDateTimeAsIMFFix(publish_time_1 - std::chrono::microseconds(1000000));
    const auto response = HTTP(PUT(base_url + "/api/data/client/" + client_key_str, updated_client)
                                   .SetHeader("If-Unmodified-Since", header_value));
    EXPECT_EQ(412, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"ResourceWasModifiedError\","
                  "\"message\":\"Resource can not be updated as it has been modified in the meantime.\","
                  "\"details\":{"
                    "\"requested_date\":\"Fri, 01 Jul 2016 08:59:59 GMT\","
                    "\"requested_us\":\"1467363599999999\","
                    "\"resource_last_modified_date\":\"Fri, 01 Jul 2016 09:00:00 GMT\","
                    "\"resource_last_modified_us\":\"1467363600000000\""
                  "}"
                "}"
              "}\n", response.body);
  }
  
  // Attempt to delete the record passing in the wrong value of the `X-Current-If-Unmodified-Since` header.
  {
    const auto response = HTTP(DELETE(base_url + "/api/data/client/" + client_key_str)
                                   .SetHeader("X-Current-If-Unmodified-Since", "Bad string"));
    EXPECT_EQ(400, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"InvalidHeader\","
                  "\"message\":\"Invalid microsecond timestamp value.\","
                  "\"details\":{"
                    "\"header\":\"X-Current-If-Unmodified-Since\","
                    "\"header_value\":\"Bad string\""
                  "}"
                "}"
              "}\n", response.body);
  }
  // Attempt to delete the record passing in the wrong value of the `If-Unmodified-Since` header.
  {
    const auto response =
        HTTP(DELETE(base_url + "/api/data/client/" + client_key_str).SetHeader("If-Unmodified-Since", "Bad string"));
    EXPECT_EQ(400, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"InvalidHeader\","
                  "\"message\":\"Unparsable datetime value.\","
                  "\"details\":{"
                    "\"header\":\"If-Unmodified-Since\","
                    "\"header_value\":\"Bad string\""
                  "}"
                "}"
              "}\n", response.body);
  }
  
  // Attempt to delete the record with the value of the `X-Current-If-Unmodified-Since` header set to 1 us in the past.
  {
    const auto header_value = current::ToString(publish_time_1 - std::chrono::microseconds(1));
    const auto response = HTTP(DELETE(base_url + "/api/data/client/" + client_key_str)
                                   .SetHeader("X-Current-If-Unmodified-Since", header_value));
    EXPECT_EQ(412, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"ResourceWasModifiedError\","
                  "\"message\":\"Resource can not be deleted as it has been modified in the meantime.\","
                  "\"details\":{"
                    "\"requested_date\":\"Fri, 01 Jul 2016 08:59:59 GMT\","
                    "\"requested_us\":\"1467363599999999\","
                    "\"resource_last_modified_date\":\"Fri, 01 Jul 2016 09:00:00 GMT\","
                    "\"resource_last_modified_us\":\"1467363600000000\""
                  "}"
                "}"
              "}\n", response.body);
  }
  // Attempt to delete the record with the value of the `If-Unmodified-Since` header set to 1 second in the past.
  {
    const auto header_value = current::FormatDateTimeAsIMFFix(publish_time_1 - std::chrono::microseconds(1000000));
    const auto response =
        HTTP(DELETE(base_url + "/api/data/client/" + client_key_str).SetHeader("If-Unmodified-Since", header_value));
    EXPECT_EQ(412, static_cast<int>(response.code));
    EXPECT_EQ("{"
                "\"success\":false,"
                "\"message\":null,"
                "\"error\":{"
                  "\"name\":\"ResourceWasModifiedError\","
                  "\"message\":\"Resource can not be deleted as it has been modified in the meantime.\","
                  "\"details\":{"
                    "\"requested_date\":\"Fri, 01 Jul 2016 08:59:59 GMT\","
                    "\"requested_us\":\"1467363599999999\","
                    "\"resource_last_modified_date\":\"Fri, 01 Jul 2016 09:00:00 GMT\","
                    "\"resource_last_modified_us\":\"1467363600000000\""
                  "}"
                "}"
              "}\n", response.body);
  }
  
  const auto update_time_1 = current::IMFFixDateTimeStringToTimestamp("Fri, 01 Jul 2016 12:00:00 GMT");
  EXPECT_EQ(1467374400000000, update_time_1.count());
  current::time::SetNow(update_time_1);
  // `PUT` with the real modification timestamp in `X-Current-If-Unmodified-Since` should pass.
  {
    const auto header_value = current::ToString(publish_time_1);
    const auto response = HTTP(PUT(base_url + "/api/data/client/" + client_key_str, updated_client)
                                   .SetHeader("X-Current-If-Unmodified-Since", header_value));
    EXPECT_EQ(200, static_cast<int>(response.code));
    ASSERT_TRUE(response.headers.Has("Last-Modified"));
    EXPECT_EQ("Fri, 01 Jul 2016 12:00:00 GMT", response.headers.Get("Last-Modified"));
    ASSERT_TRUE(response.headers.Has("X-Current-Last-Modified"));
    EXPECT_EQ("1467374400000000", response.headers.Get("X-Current-Last-Modified"));
  }
  
  // Test microsecond resolution of `X-Current-If-Unmodified-Since`.
  const auto delete_time_1 = update_time_1 + std::chrono::microseconds(1);
  current::time::SetNow(delete_time_1);
  // `DELETE` with the real modification timestamp in `X-Current-If-Unmodified-Since` should pass.
  // `X-Current-If-Unmodified-Since` has precedence over `If-Unmodified-Since`.
  {
    const auto rfc_header_value = current::FormatDateTimeAsIMFFix(publish_time_1);
    const auto current_header_value = current::ToString(update_time_1);
    const auto response = HTTP(DELETE(base_url + "/api/data/client/" + client_key_str)
                                   .SetHeader("X-Current-If-Unmodified-Since", current_header_value)
                                   .SetHeader("If-Unmodified-Since", rfc_header_value));
    EXPECT_EQ(200, static_cast<int>(response.code));
    ASSERT_TRUE(response.headers.Has("Last-Modified"));
    EXPECT_EQ("Fri, 01 Jul 2016 12:00:00 GMT", response.headers.Get("Last-Modified"));
    ASSERT_TRUE(response.headers.Has("X-Current-Last-Modified"));
    EXPECT_EQ("1467374400000001", response.headers.Get("X-Current-Last-Modified"));
  }
  
  // Restore the original client entry.
  const auto publish_time_2 = delete_time_1 + std::chrono::microseconds(1000);
  current::time::SetNow(publish_time_2);
  EXPECT_EQ(1467374400001001, publish_time_2.count());
  {
    const auto post_response = HTTP(POST(base_url + "/api/data/client", Client(ClientID(42))));
    EXPECT_EQ(201, static_cast<int>(post_response.code));
    // Check `Last-Modified`.
    ASSERT_TRUE(post_response.headers.Has("Last-Modified"));
    const auto expected_date = current::FormatDateTimeAsIMFFix(publish_time_2);
    ASSERT_EQ("Fri, 01 Jul 2016 12:00:00 GMT", expected_date);
    EXPECT_EQ(expected_date, post_response.headers.Get("Last-Modified"));
    // Check `X-Current-Last-Modified`.
    ASSERT_TRUE(post_response.headers.Has("X-Current-Last-Modified"));
    const auto expected_us_string = current::ToString(publish_time_2);
    ASSERT_EQ("1467374400001001", expected_us_string);
    EXPECT_EQ(expected_us_string, post_response.headers.Get("X-Current-Last-Modified"));
  }
  
  const auto update_time_2 = current::IMFFixDateTimeStringToTimestamp("Fri, 01 Jul 2016 14:00:00 GMT");
  EXPECT_EQ(1467381600000000, update_time_2.count());
  current::time::SetNow(update_time_2);
  // `PUT` with the real modification timestamp in `If-Unmodified-Since` should pass.
  {
    const auto header_value = current::FormatDateTimeAsIMFFix(publish_time_2);
    const auto response = HTTP(PUT(base_url + "/api/data/client/" + client_key_str, updated_client)
                                   .SetHeader("If-Unmodified-Since", header_value));
    EXPECT_EQ(200, static_cast<int>(response.code));
    ASSERT_TRUE(response.headers.Has("Last-Modified"));
    EXPECT_EQ("Fri, 01 Jul 2016 14:00:00 GMT", response.headers.Get("Last-Modified"));
    ASSERT_TRUE(response.headers.Has("X-Current-Last-Modified"));
    EXPECT_EQ("1467381600000000", response.headers.Get("X-Current-Last-Modified"));
  }
  
  const auto delete_time_2 = current::IMFFixDateTimeStringToTimestamp("Fri, 01 Jul 2016 15:00:00 GMT");
  EXPECT_EQ(1467385200000000, delete_time_2.count());
  current::time::SetNow(delete_time_2);
  // `DELETE` with the real modification timestamp in `If-Unmodified-Since` should pass.
  {
    const auto header_value = current::FormatDateTimeAsIMFFix(update_time_2);
    const auto response =
        HTTP(DELETE(base_url + "/api/data/client/" + client_key_str).SetHeader("If-Unmodified-Since", header_value));
    EXPECT_EQ(200, static_cast<int>(response.code));
    ASSERT_TRUE(response.headers.Has("Last-Modified"));
    EXPECT_EQ("Fri, 01 Jul 2016 15:00:00 GMT", response.headers.Get("Last-Modified"));
    ASSERT_TRUE(response.headers.Has("X-Current-Last-Modified"));
    EXPECT_EQ("1467385200000000", response.headers.Get("X-Current-Last-Modified"));
  }
  
}

namespace storage_docu {

  // Example of custom REST implementation that annotates transactions.
  struct RESTWithMeta : current::storage::rest::Hypermedia {
    using SUPER = current::storage::rest::Hypermedia;

    template <class HTTP_VERB, typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
    struct RESTfulDataHandler : SUPER::RESTfulDataHandler<HTTP_VERB, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
      using ACTUAL_SUPER = SUPER::RESTfulDataHandler<HTTP_VERB, OPERATION, PARTICULAR_FIELD, ENTRY, KEY>;

      template <class INPUT, bool IS_GET = std::is_same<HTTP_VERB, GET>::value>
      std::enable_if_t<!IS_GET, Response> Run(const INPUT& input) const {
        input.fields.SetTransactionMetaField("who", "unittest");
        return this->ACTUAL_SUPER::template Run<INPUT>(input);
      }

      template <class INPUT, bool IS_GET = std::is_same<HTTP_VERB, GET>::value>
      std::enable_if_t<IS_GET, Response> Run(const INPUT& input) const {
        return this->ACTUAL_SUPER::template Run<INPUT>(input);
      }
    };
  };
  
}  // namespace storage_docu

TEST(StorageDocumentation, RESTFillingTransactionMetaExample) {
  // The `RESTFillingTransactionMetaExample` test.
  current::time::ResetToZero();
  
using namespace current::storage::rest;
using namespace storage_docu;
  using TestStorage = StorageOfClients<SherlockStreamPersister>;
  
  const std::string client_storage_file_name =
      current::FileSystem::JoinPath(FLAGS_client_storage_test_tmpdir, "client_with_meta");
const auto client_storage_file_remover = current::FileSystem::ScopedRmFile(client_storage_file_name);
  auto storage = TestStorage::CreateMasterStorage(client_storage_file_name);
  
  const auto rest = RESTfulStorage<TestStorage, RESTWithMeta>(
      *storage, FLAGS_client_storage_test_port, "/api", "http://example.current.ai/api");
  const auto base_url = current::strings::Printf("http://localhost:%d", FLAGS_client_storage_test_port);
  
  // Add client.
  current::time::SetNow(std::chrono::microseconds(1024));
  EXPECT_EQ(201, static_cast<int>(HTTP(PUT(base_url + "/api/data/client/101", Client(ClientID(101)))).code));
  // Delete client.
  current::time::SetNow(std::chrono::microseconds(2000));
  EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api/data/client/101")).code));
  
  // Check that everything has been persisted correctly, including meta fields.
  const auto persisted_entries = current::strings::Split<current::strings::ByLines>(
      current::FileSystem::ReadFileAsString(client_storage_file_name));
  // 1 line with the signature and 2 with the entries.
  ASSERT_EQ(3u, persisted_entries.size());
  
  {
    const auto add_fields = current::strings::Split(persisted_entries[1], '\t');
    ASSERT_TRUE(add_fields.size() == 2u);
    const auto idx_ts = ParseJSON<idxts_t>(add_fields[0]);
    EXPECT_EQ(0u, idx_ts.index);
    EXPECT_EQ(1024, idx_ts.us.count());
    const auto add_transaction = ParseJSON<TestStorage::transaction_t>(add_fields[1]);
    EXPECT_EQ(1024, add_transaction.meta.begin_us.count());
    EXPECT_EQ(1024, add_transaction.meta.end_us.count());
    ASSERT_EQ(1u, add_transaction.meta.fields.size());
    EXPECT_EQ("unittest", add_transaction.meta.fields.at("who"));
    ASSERT_EQ(1u, add_transaction.mutations.size());

    ASSERT_TRUE(Exists<PersistedClientUpdated>(add_transaction.mutations[0]));
    const auto& client = Value<PersistedClientUpdated>(add_transaction.mutations[0]).data;
    EXPECT_EQ(ClientID(101), client.key);
    EXPECT_EQ("John Doe", client.name);
    EXPECT_TRUE(client.white);
    EXPECT_TRUE(client.straight);
    EXPECT_TRUE(client.male);
  }
  
  {
    const auto del_fields = current::strings::Split(persisted_entries[2], '\t');
    ASSERT_TRUE(del_fields.size() == 2u);
    const auto idx_ts = ParseJSON<idxts_t>(del_fields[0]);
    EXPECT_EQ(1u, idx_ts.index);
    EXPECT_EQ(2000, idx_ts.us.count());
    const auto del_transaction = ParseJSON<TestStorage::transaction_t>(del_fields[1]);
    EXPECT_EQ(2000, del_transaction.meta.begin_us.count());
    EXPECT_EQ(2000, del_transaction.meta.end_us.count());
    ASSERT_EQ(1u, del_transaction.meta.fields.size());
    EXPECT_EQ("unittest", del_transaction.meta.fields.at("who"));
    ASSERT_EQ(1u, del_transaction.mutations.size());
  }
}

#endif  // CURRENT_STORAGE_DOCU_DOCU_3_CODE_CC

// clang-format on
