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

#include "../test_helpers.cc"

TEST(TransactionalStorage, RESTfulAPITest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using storage_t = SimpleStorage<StreamStreamPersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  EXPECT_EQ(6u, storage_t::FIELDS_COUNT);
  auto stream = storage_t::stream_t::CreateStream(persistence_file_name);
  auto storage = storage_t::CreateMasterStorageAtopExistingStream(stream);

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  const auto base_url = current::strings::Printf("http://localhost:%d", port);

  // clang-format off
  const std::string golden_user_schema_h =
      "// The `current.h` file is the one from `https://github.com/C5T/Current`.\n"
      "// Compile with `-std=c++11` or higher.\n"
      "\n"
      "#include \"current.h\"\n"
      "\n"
      "// clang-format off\n"
      "\n"
      "namespace current_userspace {\n"
      "\n"
      "#ifndef CURRENT_SCHEMA_FOR_T9204302787243580577\n"
      "#define CURRENT_SCHEMA_FOR_T9204302787243580577\n"
      "namespace t9204302787243580577 {\n"
      "CURRENT_STRUCT(SimpleUser) {\n"
      "  CURRENT_FIELD(key, std::string);\n"
      "  CURRENT_FIELD(name, std::string);\n"
      "};\n"
      "}  // namespace t9204302787243580577\n"
      "#endif  // CURRENT_SCHEMA_FOR_T_9204302787243580577\n"
      "\n"
      "}  // namespace current_userspace\n"
      "\n"
      "#ifndef CURRENT_NAMESPACE_USERSPACE_C546BDBF8F3A5614_DEFINED\n"
      "#define CURRENT_NAMESPACE_USERSPACE_C546BDBF8F3A5614_DEFINED\n"
      "CURRENT_NAMESPACE(USERSPACE_C546BDBF8F3A5614) {\n"
      "  CURRENT_NAMESPACE_TYPE(SimpleUser, current_userspace::t9204302787243580577::SimpleUser);\n"
      "};  // CURRENT_NAMESPACE(USERSPACE_C546BDBF8F3A5614)\n"
      "#endif  // CURRENT_NAMESPACE_USERSPACE_C546BDBF8F3A5614_DEFINED\n"
      "\n"
      "namespace current {\n"
      "namespace type_evolution {\n"
      "\n"
      "// Default evolution for struct `SimpleUser`.\n"
      "#ifndef DEFAULT_EVOLUTION_03EF01DB8AAC8284F79033E453C68C55958D143788D6A43EC1F8ED9B22050D10  // typename USERSPACE_C546BDBF8F3A5614::SimpleUser\n"
      "#define DEFAULT_EVOLUTION_03EF01DB8AAC8284F79033E453C68C55958D143788D6A43EC1F8ED9B22050D10  // typename USERSPACE_C546BDBF8F3A5614::SimpleUser\n"
      "template <typename CURRENT_ACTIVE_EVOLVER>\n"
      "struct Evolve<USERSPACE_C546BDBF8F3A5614, typename USERSPACE_C546BDBF8F3A5614::SimpleUser, CURRENT_ACTIVE_EVOLVER> {\n"
      "  using FROM = USERSPACE_C546BDBF8F3A5614;\n"
      "  template <typename INTO>\n"
      "  static void Go(const typename FROM::SimpleUser& from,\n"
      "                 typename INTO::SimpleUser& into) {\n"
      "      static_assert(::current::reflection::FieldCounter<typename INTO::SimpleUser>::value == 2,\n"
      "                    \"Custom evolver required.\");\n"
      "      CURRENT_COPY_FIELD(key);\n"
      "      CURRENT_COPY_FIELD(name);\n"
      "  }\n"
      "};\n"
      "#endif\n"
      "\n"
      "}  // namespace current::type_evolution\n"
      "}  // namespace current\n"
      "\n"
      "#if 0  // Boilerplate evolvers.\n"
      "\n"
      "CURRENT_STRUCT_EVOLVER(CustomEvolver, USERSPACE_C546BDBF8F3A5614, SimpleUser, {\n"
      "  CURRENT_COPY_FIELD(key);\n"
      "  CURRENT_COPY_FIELD(name);\n"
      "});\n"
      "\n"
      "#endif  // Boilerplate evolvers.\n"
      "\n"
      "// clang-format on\n";
  const std::string golden_user_schema_json =
  "["
    "{\"object\":\"SimpleUser\",\"contains\":["
      "{\"field\":\"key\",\"as\":{\"primitive\":{\"type\":\"std::string\",\"text\":\"String\"}}},"
      "{\"field\":\"name\",\"as\":{\"primitive\":{\"type\":\"std::string\",\"text\":\"String\"}}}"
    "]}"
  "]";
  // clang-format on

  // Run twice to make sure the `GET-POST-GET-DELETE` cycle is complete.
  for (size_t i = 0; i < 2; ++i) {
    // Register RESTful HTTP endpoints, in a scoped way.
    auto rest = RESTfulStorage<storage_t>(*storage, port, "/api_plain", "http://unittest.current.ai");
    const auto hypermedia_rest = RESTfulStorage<storage_t, current::storage::rest::Hypermedia>(
        *storage, port, "/api_hypermedia", "http://unittest.current.ai");

    // Confirm the schema is returned.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/schema/user")).code));
    EXPECT_EQ("SimpleUser", HTTP(GET(base_url + "/api_plain/schema/user")).body);
    EXPECT_EQ(golden_user_schema_h, HTTP(GET(base_url + "/api_plain/schema/user.h")).body);
    EXPECT_EQ(golden_user_schema_json, HTTP(GET(base_url + "/api_plain/schema/user.json")).body);
    EXPECT_EQ("SimplePost", HTTP(GET(base_url + "/api_plain/schema/post")).body);
    EXPECT_EQ("SimpleLike", HTTP(GET(base_url + "/api_plain/schema/like")).body);
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/schema/user_alias")).code));

    rest.RegisterAlias("user", "user_alias");

    // Confirm the schema is returned as the alias too.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/schema/user_alias")).code));
    EXPECT_EQ("SimpleUser", HTTP(GET(base_url + "/api_plain/schema/user_alias")).body);
    EXPECT_EQ(golden_user_schema_h, HTTP(GET(base_url + "/api_plain/schema/user_alias.h")).body);

    // Confirm an empty collection is returned.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/user")).code));
    EXPECT_EQ("", HTTP(GET(base_url + "/api_plain/data/user")).body);

    // Begin POST-ing data.
    auto user1 = SimpleUser("max", "MZ");
    auto user2 = SimpleUser("dima", "DK");

    // POST first user via Plain API.
    const auto post_user1_response = HTTP(POST(base_url + "/api_plain/data/user", user1));
    ASSERT_EQ(201, static_cast<int>(post_user1_response.code));
    const auto user1_key = post_user1_response.body;
    user1.key = user1_key;  // Assigned by the server, save for the purposes of this test.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/user/" + user1_key)).code));
    EXPECT_EQ("MZ", ParseJSON<SimpleUser>(HTTP(GET(base_url + "/api_plain/data/user/" + user1_key)).body).name);

    // Test the alias too.
    EXPECT_EQ("MZ", ParseJSON<SimpleUser>(HTTP(GET(base_url + "/api_plain/data/user_alias/" + user1_key)).body).name);

    // Test other key format too.
    EXPECT_EQ("MZ", ParseJSON<SimpleUser>(HTTP(GET(base_url + "/api_plain/data/user?key=" + user1_key)).body).name);

    // Confirm a collection of one element is returned.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/user")).code));
    EXPECT_EQ(user1_key + '\t' + JSON(user1) + '\n', HTTP(GET(base_url + "/api_plain/data/user")).body);

    // POST second user via Hypermedia API.
    const auto post_user2_response = HTTP(POST(base_url + "/api_hypermedia/data/user", user2));
    ASSERT_EQ(201, static_cast<int>(post_user2_response.code));
    const std::string user2_resource_url = Value(
        ParseJSON<current::storage::rest::generic::RESTResourceUpdateResponse>(post_user2_response.body).resource_url);
    const std::string user2_key = user2_resource_url.substr(user2_resource_url.rfind('/') + 1);
    user2.key = user2_key;  // Assigned by the server, save for the purposes of this test.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_hypermedia/data/user")).code));

    // Test collection retrieval.
    EXPECT_EQ(user1_key + '\t' + JSON(user1) + '\n' + user2_key + '\t' + JSON(user2) + '\n',
              HTTP(GET(base_url + "/api_plain/data/user")).body);

    // Confirm matrix collection retrieval.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/user.key")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/user.row")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.row")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.col")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.key")).code));

    // Non-GET methods are not allowed for partial key accessors.
    EXPECT_EQ(405, static_cast<int>(HTTP(POST(base_url + "/api_plain/data/like.row", SimpleLike("x", "y"))).code));
    EXPECT_EQ(405, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/like.row/x")).code));

    {
      // Test PUT on users.
      EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      {
        user2.name = "dk2";
        EXPECT_NE(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
        EXPECT_EQ(200, static_cast<int>(HTTP(PUT(base_url + "/api_plain/data/user/" + user2_key, user2)).code));
        EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      }
      {
        EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
        const auto response = HTTP(PUT(base_url + "/api_plain/data/user/" + user2_key, SimpleUser("dima2a", "dk2a")));
        EXPECT_EQ(400, static_cast<int>(response.code));
        EXPECT_EQ("The object key doesn't match the URL key.\n", response.body);
        EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      }
    }

    {
      // Test PATCH on users.
      EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      {
        user2.name = "dk3";
        EXPECT_NE(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
        EXPECT_EQ(200,
                  static_cast<int>(
                      HTTP(PATCH(base_url + "/api_plain/data/user/" + user2_key, SimpleUserValidPatch("dk3"))).code));
        EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      }
      {
        const auto response = HTTP(PATCH(base_url + "/api_plain/data/user/" + user2_key, SimpleUser("dima3a", "foo")));
        EXPECT_EQ(400, static_cast<int>(response.code));
        EXPECT_EQ("PATCH should not change the key.\n", response.body);
        EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      }
      {
        const auto response = HTTP(PATCH(base_url + "/api_plain/data/user/" + user2_key, SimpleUserInvalidPatch()));
        EXPECT_EQ(400, static_cast<int>(response.code));
        EXPECT_EQ("Bad JSON.\n", response.body);
        EXPECT_EQ(JSON(user2) + '\n', HTTP(GET(base_url + "/api_plain/data/user/" + user2_key)).body);
      }
    }

    // Delete the users.
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/user/" + user1_key)).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/user/" + user2_key)).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/user/max")).code));

    // Run the subset of the above test for posts, not just for users.
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/post/test")).code));

    EXPECT_EQ(201,
              static_cast<int>(HTTP(PUT(base_url + "/api_plain/data/post/test", SimplePost("test", "blah"))).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/post/test")).code));
    EXPECT_EQ("blah", ParseJSON<SimplePost>(HTTP(GET(base_url + "/api_plain/data/post/test")).body).text);

    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/post/test")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/post/test")).code));

    // Test RESTful matrix too.
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/dima/beer")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/max/beer")).code));

    // For this test, we disallow POST for the `/like`-s matrix.
    EXPECT_EQ(405, static_cast<int>(HTTP(POST(base_url + "/api_plain/data/like", SimpleLike("dima", "beer"))).code));

    // Add a few likes.
    EXPECT_EQ(201,
              static_cast<int>(
                  HTTP(PUT(base_url + "/api_plain/data/like/dima/beer", SimpleLike("dima", "beer", "Yo!"))).code));
    EXPECT_EQ(
        201,
        static_cast<int>(HTTP(PUT(base_url + "/api_hypermedia/data/like/max/beer", SimpleLike("max", "beer"))).code));

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_hypermedia/data/like/dima/beer")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/max/beer")).code));

    {
      // Test PATCH on likes.
      {
        EXPECT_EQ(200,
                  static_cast<int>(HTTP(PATCH(base_url + "/api_plain/data/like/max/beer",
                                              SimpleLikeValidPatch(std::string("Cheers!"))))
                                       .code));
        EXPECT_EQ(
            400,
            static_cast<int>(HTTP(PATCH(base_url + "/api_plain/data/like/max/beer", SimpleLikeInvalidPatch())).code));
      }
      {
        EXPECT_EQ(
            200,
            static_cast<int>(
                HTTP(PATCH(base_url + "/api_hypermedia/data/like/dima/beer", SimpleLikeValidPatch(nullptr))).code));
        EXPECT_EQ(400,
                  static_cast<int>(
                      HTTP(PATCH(base_url + "/api_hypermedia/data/like/dima/beer", SimpleLikeInvalidPatch())).code));
      }
    }

    // Confirm the patches went through.
    EXPECT_EQ("{\"row\":\"dima\",\"col\":\"beer\",\"details\":null}\n",
              HTTP(GET(base_url + "/api_plain/data/like/dima/beer")).body);
    EXPECT_EQ("{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}\n",
              HTTP(GET(base_url + "/api_plain/data/like?row=max&col=beer")).body);
    EXPECT_EQ("{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}\n",
              HTTP(GET(base_url + "/api_plain/data/like?key1=max&key2=beer")).body);
    EXPECT_EQ("{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}\n",
              HTTP(GET(base_url + "/api_plain/data/like/max/beer")).body);

    // Confirm the likes are returned correctly both in brief and full formats.
    EXPECT_EQ(HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer")).body,
              HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=full")).body);
    EXPECT_EQ(HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?full")).body,
              HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=full")).body);
    EXPECT_EQ(HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?brief")).body,
              HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=brief")).body);
    EXPECT_NE(HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=full")).body,
              HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=brief")).body);

    EXPECT_EQ(
        "{\"success\":true,\"url\":\"http://unittest.current.ai/data/like/max/beer\",\"url_full\":\"http://"
        "unittest.current.ai/data/like/max/beer?full\",\"url_brief\":\"http://unittest.current.ai/data/like/"
        "max/beer?brief\",\"url_directory\":\"http://unittest.current.ai/data/"
        "like\",\"data\":{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}}\n",
        HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer")).body);
    EXPECT_EQ(
        "{\"success\":true,\"url\":\"http://unittest.current.ai/data/like/max/beer\",\"url_full\":\"http://"
        "unittest.current.ai/data/like/max/beer?full\",\"url_brief\":\"http://unittest.current.ai/data/like/"
        "max/beer?brief\",\"url_directory\":\"http://unittest.current.ai/data/"
        "like\",\"data\":{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}}\n",
        HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=full")).body);
    EXPECT_EQ(
        "{\"success\":true,\"url\":\"http://unittest.current.ai/data/like/max/beer\",\"url_full\":\"http://"
        "unittest.current.ai/data/like/max/beer?full\",\"url_brief\":\"http://unittest.current.ai/data/like/"
        "max/beer?brief\",\"url_directory\":\"http://unittest.current.ai/data/"
        "like\",\"data\":{\"row\":\"max\",\"col\":\"beer\"}}\n",
        HTTP(GET(base_url + "/api_hypermedia/data/like/max/beer?fields=brief")).body);

    // GET matrix as the collection, all records.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like")).code));
    EXPECT_EQ(
        "max\tbeer\t{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}\n"
        "dima\tbeer\t{\"row\":\"dima\",\"col\":\"beer\",\"details\":null}\n",
        HTTP(GET(base_url + "/api_plain/data/like")).body);

    // GET matrix as the collection, per rows.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.row")).code));
    EXPECT_EQ("max\t1\ndima\t1\n", HTTP(GET(base_url + "/api_plain/data/like.row")).body);

    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.row/foo")).code));
    EXPECT_EQ("Nope.\n", HTTP(GET(base_url + "/api_plain/data/like.row/foo")).body);

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.row/dima")).code));
    EXPECT_EQ("{\"row\":\"dima\",\"col\":\"beer\",\"details\":null}\n",
              HTTP(GET(base_url + "/api_plain/data/like.row/dima")).body);

    // GET matrix as the collection, per cols.
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.col")).code));
    EXPECT_EQ("beer\t2\n", HTTP(GET(base_url + "/api_plain/data/like.col")).body);

    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.col/foo")).code));
    EXPECT_EQ("Nope.\n", HTTP(GET(base_url + "/api_plain/data/like.col/foo")).body);

    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like.col/beer")).code));
    EXPECT_EQ(
        "{\"row\":\"max\",\"col\":\"beer\",\"details\":\"Cheers!\"}\n"
        "{\"row\":\"dima\",\"col\":\"beer\",\"details\":null}\n",
        HTTP(GET(base_url + "/api_plain/data/like.col/beer")).body);

    // Clean up the likes.
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/like/dima/beer")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/like/max/beer")).code));

    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/dima/beer")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/max/beer")).code));

    // Confirm REST endpoints successfully change to 503s.
    rest.SwitchHTTPEndpointsTo503s();
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api_plain/data/post", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api_plain/data/post/test", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/blah/blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api_plain/data/like", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api_plain/data/like/blah/blah", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/like/blah/blah")).code));

    // Twice, just in case.
    rest.SwitchHTTPEndpointsTo503s();
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api_plain/data/post", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api_plain/data/post/test", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/post/test")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api_plain/data/like/blah/blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(POST(base_url + "/api_plain/data/like", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(PUT(base_url + "/api_plain/data/like/blah/blah", "blah")).code));
    EXPECT_EQ(503, static_cast<int>(HTTP(DELETE(base_url + "/api_plain/data/like/blah/blah")).code));
  }

  // DIMA: Persister?
  // EXPECT_EQ(28u, storage->Size());
}

TEST(TransactionalStorage, RESTfulAPIMatrixTest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using storage_t = SimpleStorage<StreamInMemoryStreamPersister>;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  auto storage = storage_t::CreateMasterStorage();

  const auto base_url = current::strings::Printf("http://localhost:%d", port);

  const auto rest1 = RESTfulStorage<storage_t>(*storage, port, "/plain", "");
  const auto rest2 = RESTfulStorage<storage_t, current::storage::rest::Simple>(*storage, port, "/simple", "");
  const auto rest3 = RESTfulStorage<storage_t, current::storage::rest::Hypermedia>(*storage, port, "/hypermedia", "");

  {
    // Create { "!1", "!2", "!3" } x { 1, 2, 3 }, excluding the main diagonal.
    // Try all three REST implementations, as well as both POST and PUT.
    EXPECT_EQ(201,
              static_cast<int>(HTTP(POST(base_url + "/plain/data/composite_m2m",
                                         SimpleComposite("!1", std::chrono::microseconds(2))))
                                   .code));
    EXPECT_EQ(201,
              static_cast<int>(HTTP(POST(base_url + "/simple/data/composite_m2m",
                                         SimpleComposite("!1", std::chrono::microseconds(3))))
                                   .code));
    EXPECT_EQ(201,
              static_cast<int>(HTTP(POST(base_url + "/hypermedia/data/composite_m2m",
                                         SimpleComposite("!2", std::chrono::microseconds(1))))
                                   .code));
    EXPECT_EQ(201,
              static_cast<int>(HTTP(PUT(base_url + "/plain/data/composite_m2m/!2/3",
                                        SimpleComposite("!2", std::chrono::microseconds(3))))
                                   .code));
    EXPECT_EQ(201,
              static_cast<int>(HTTP(PUT(base_url + "/simple/data/composite_m2m/!3/1",
                                        SimpleComposite("!3", std::chrono::microseconds(1))))
                                   .code));
    EXPECT_EQ(201,
              static_cast<int>(HTTP(PUT(base_url + "/hypermedia/data/composite_m2m/!3/2",
                                        SimpleComposite("!3", std::chrono::microseconds(2))))
                                   .code));
  }

  {// Browse the collection in various ways using the `Plain` API.
   {const auto response = HTTP(GET(base_url + "/plain/data/composite_m2m"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  // The top-level container is still unordered, so meh.
  std::vector<std::string> elements = current::strings::Split<current::strings::ByLines>(response.body);
  std::sort(elements.begin(), elements.end());
  EXPECT_EQ(
      "!1\t2\t{\"row\":\"!1\",\"col\":2}\n"
      "!1\t3\t{\"row\":\"!1\",\"col\":3}\n"
      "!2\t1\t{\"row\":\"!2\",\"col\":1}\n"
      "!2\t3\t{\"row\":\"!2\",\"col\":3}\n"
      "!3\t1\t{\"row\":\"!3\",\"col\":1}\n"
      "!3\t2\t{\"row\":\"!3\",\"col\":2}",
      current::strings::Join(elements, '\n'));
}
{
  const auto response = HTTP(GET(base_url + "/plain/data/composite_m2m.row"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("!1\t2\n!2\t2\n!3\t2\n", response.body);
}
{
  const auto response = HTTP(GET(base_url + "/plain/data/composite_m2m.col"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("1\t2\n2\t2\n3\t2\n", response.body);
}
{
  const auto response = HTTP(GET(base_url + "/plain/data/composite_m2m.row/!2"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"row\":\"!2\",\"col\":1}\n{\"row\":\"!2\",\"col\":3}\n", response.body);
}
{
  const auto response = HTTP(GET(base_url + "/plain/data/composite_m2m.col/3"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("{\"row\":\"!1\",\"col\":3}\n{\"row\":\"!2\",\"col\":3}\n", response.body);
}
}

{// Browse the collection in various ways using the `Simple` API.
 {const auto response = HTTP(GET(base_url + "/simple/data/composite_m2m"));
EXPECT_EQ(200, static_cast<int>(response.code));
using parsed_t = current::storage::rest::simple::SimpleRESTContainerResponse;
parsed_t parsed;
ASSERT_NO_THROW(ParseJSON<parsed_t>(response.body, parsed));
// Don't test the body of `"/simple/data/composite_m2m"`, it's unordered and machine-dependent.
std::vector<std::string> strings;
for (const auto& resource : parsed.data) {
  strings.push_back(JSON(resource));
}
std::sort(strings.begin(), strings.end());
EXPECT_EQ(
    "\"/data/composite_m2m/!1/2\"\n"
    "\"/data/composite_m2m/!1/3\"\n"
    "\"/data/composite_m2m/!2/1\"\n"
    "\"/data/composite_m2m/!2/3\"\n"
    "\"/data/composite_m2m/!3/1\"\n"
    "\"/data/composite_m2m/!3/2\"",
    current::strings::Join(strings, '\n'));
}
{
  const auto response = HTTP(GET(base_url + "/simple/data/composite_m2m.row"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"message\":null,\"error\":null,\"url\":\"/data/composite_m2m.1\",\"data\":[\"/"
      "data/composite_m2m.1/!1\",\"/data/composite_m2m.1/!2\",\"/data/composite_m2m.1/!3\"]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/simple/data/composite_m2m.col"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"message\":null,\"error\":null,\"url\":\"/data/composite_m2m.2\",\"data\":[\"/"
      "data/composite_m2m.2/1\",\"/data/composite_m2m.2/2\",\"/data/composite_m2m.2/3\"]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/simple/data/composite_m2m.1/!2"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"message\":null,\"error\":null,\"url\":\"/data/composite_m2m.1/!2\",\"data\":[\"/data/"
      "composite_m2m/!2/1\",\"/data/composite_m2m/!2/3\"]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/simple/data/composite_m2m.2/3"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"message\":null,\"error\":null,\"url\":\"/data/composite_m2m.2/3\",\"data\":[\"/data/"
      "composite_m2m/!1/3\",\"/data/composite_m2m/!2/3\"]}\n",
      response.body);
}
}
{// Browse the collection in various ways using the `Hypermedia` API.
 {const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m"));
EXPECT_EQ(200, static_cast<int>(response.code));
using parsed_t =
    hypermedia::HypermediaRESTCollectionResponse<hypermedia::HypermediaRESTFullCollectionRecord<SimpleComposite>>;
parsed_t parsed;
ASSERT_NO_THROW(ParseJSON<parsed_t>(response.body, parsed));
std::vector<std::string> strings;
for (const auto& resource : parsed.data) {
  strings.push_back(JSON(resource.data));
}
std::sort(strings.begin(), strings.end());
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.1"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.1?i=0&n=10\",\"url_directory\":\"/data/"
      "composite_m2m.1\",\"i\":0,\"n\":3,\"total\":3,\"url_next_page\":null,\"url_previous_page\":null,"
      "\"data\":[{\"url\":\"/data/composite_m2m.1/"
      "!1\",\"data\":{\"total\":2,\"preview\":[{\"row\":\"!1\",\"col\":2},{\"row\":\"!1\",\"col\":3}]}},{"
      "\"url\":\"/data/composite_m2m.1/"
      "!2\",\"data\":{\"total\":2,\"preview\":[{\"row\":\"!2\",\"col\":1},{\"row\":\"!2\",\"col\":3}]}},{"
      "\"url\":\"/data/composite_m2m.1/"
      "!3\",\"data\":{\"total\":2,\"preview\":[{\"row\":\"!3\",\"col\":1},{\"row\":\"!3\",\"col\":2}]}}]}"
      "\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.2"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.2?i=0&n=10\",\"url_directory\":\"/data/"
      "composite_m2m.2\",\"i\":0,\"n\":3,\"total\":3,\"url_next_page\":null,\"url_previous_page\":null,"
      "\"data\":[{\"url\":\"/data/composite_m2m.2/"
      "1\",\"data\":{\"total\":2,\"preview\":[{\"row\":\"!2\",\"col\":1},{\"row\":\"!3\",\"col\":1}]}},{"
      "\"url\":\"/data/composite_m2m.2/"
      "2\",\"data\":{\"total\":2,\"preview\":[{\"row\":\"!1\",\"col\":2},{\"row\":\"!3\",\"col\":2}]}},{"
      "\"url\":\"/data/composite_m2m.2/"
      "3\",\"data\":{\"total\":2,\"preview\":[{\"row\":\"!1\",\"col\":3},{\"row\":\"!2\",\"col\":3}]}}]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.1/!2"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.1/!2?i=0&n=10\",\"url_directory\":\"/data/"
      "composite_m2m\",\"i\":0,\"n\":2,\"total\":2,\"url_next_page\":null,\"url_previous_page\":null,"
      "\"data\":[{\"url\":\"/data/composite_m2m/!2/1\",\"data\":{\"row\":\"!2\",\"col\":1}},{\"url\":\"/"
      "data/composite_m2m/!2/3\",\"data\":{\"row\":\"!2\",\"col\":3}}]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.2/3"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.2/3?i=0&n=10\",\"url_directory\":\"/data/"
      "composite_m2m\",\"i\":0,\"n\":2,\"total\":2,\"url_next_page\":null,\"url_previous_page\":null,"
      "\"data\":[{\"url\":\"/data/composite_m2m/!1/3\",\"data\":{\"row\":\"!1\",\"col\":3}},{\"url\":\"/"
      "data/composite_m2m/!2/3\",\"data\":{\"row\":\"!2\",\"col\":3}}]}\n",
      response.body);
}
// And some inner-level pagination tests.
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.1/!2?n=1"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.1/!2?i=0&n=1\",\"url_directory\":\"/data/"
      "composite_m2m\",\"i\":0,\"n\":1,\"total\":2,\"url_next_page\":\"/data/composite_m2m.1/"
      "!2?i=1&n=1\",\"url_previous_page\":null,\"data\":[{\"url\":\"/data/composite_m2m/!2/"
      "1\",\"data\":{\"row\":\"!2\",\"col\":1}}]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.1/!2?n=1&i=1"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.1/!2?i=1&n=1\",\"url_directory\":\"/data/"
      "composite_m2m\",\"i\":1,\"n\":1,\"total\":2,\"url_next_page\":null,\"url_previous_page\":\"/data/"
      "composite_m2m.1/!2?i=0&n=1\",\"data\":[{\"url\":\"/data/composite_m2m/!2/"
      "3\",\"data\":{\"row\":\"!2\",\"col\":3}}]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.2/3?n=1"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.2/3?i=0&n=1\",\"url_directory\":\"/data/"
      "composite_m2m\",\"i\":0,\"n\":1,\"total\":2,\"url_next_page\":\"/data/composite_m2m.2/"
      "3?i=1&n=1\",\"url_previous_page\":null,\"data\":[{\"url\":\"/data/composite_m2m/!1/"
      "3\",\"data\":{\"row\":\"!1\",\"col\":3}}]}\n",
      response.body);
}
{
  const auto response = HTTP(GET(base_url + "/hypermedia/data/composite_m2m.2/3?n=1&i=1"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(
      "{\"success\":true,\"url\":\"/data/composite_m2m.2/3?i=1&n=1\",\"url_directory\":\"/data/"
      "composite_m2m\",\"i\":1,\"n\":1,\"total\":2,\"url_next_page\":null,\"url_previous_page\":\"/data/"
      "composite_m2m.2/3?i=0&n=1\",\"data\":[{\"url\":\"/data/composite_m2m/!2/"
      "3\",\"data\":{\"row\":\"!2\",\"col\":3}}]}\n",
      response.body);
}
}

{
  // Test DELETE too.
  {
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/plain/data/composite_m2m/!1/2")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/plain/data/composite_m2m/!1/2")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/plain/data/composite_m2m/!1/2")).code));
  }
  {
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/simple/data/composite_m2m/!2/3")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/simple/data/composite_m2m/!2/3")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/simple/data/composite_m2m/!2/3")).code));
  }
  {
    EXPECT_EQ(200, static_cast<int>(HTTP(GET(base_url + "/hypermedia/data/composite_m2m/!3/1")).code));
    EXPECT_EQ(200, static_cast<int>(HTTP(DELETE(base_url + "/hypermedia/data/composite_m2m/!3/1")).code));
    EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/hypermedia/data/composite_m2m/!3/1")).code));
  }
}
}

TEST(TransactionalStorage, CQSTest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using storage_t = SimpleStorage<StreamInMemoryStreamPersister>;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  auto storage = storage_t::CreateMasterStorage();

  const auto base_url = current::strings::Printf("http://localhost:%d", port);
  auto storage_http_interface =
      RESTfulStorage<storage_t, current::storage::rest::Simple>(*storage, port, "/api", "http://unittest.current.ai");

  {
    const std::string user_key = ([&]() {
      const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("dima", "DK")));
      EXPECT_EQ(201, static_cast<int>(post_response.code));
      return current::strings::Split(
                 Value(ParseJSON<current::storage::rest::generic::RESTResourceUpdateResponse>(post_response.body)
                           .resource_url),
                 '/')
          .back();
    })();

    {
      const auto get_response = HTTP(GET(base_url + "/api/data/user/" + user_key));
      EXPECT_EQ(200, static_cast<int>(get_response.code));
      EXPECT_EQ(
          "DK",
          ParseJSON<current::storage::rest::simple::SimpleRESTRecordResponse<SimpleUser>>(get_response.body).data.name);
    }
  }

  {
    const std::string user_key = ([&]() {
      const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("max", "MZ")));
      EXPECT_EQ(201, static_cast<int>(post_response.code));
      return current::strings::Split(
                 Value(ParseJSON<current::storage::rest::generic::RESTResourceUpdateResponse>(post_response.body)
                           .resource_url),
                 '/')
          .back();
    })();

    {
      const auto get_response = HTTP(GET(base_url + "/api/data/user/" + user_key));
      EXPECT_EQ(200, static_cast<int>(get_response.code));
      EXPECT_EQ(
          "MZ",
          ParseJSON<current::storage::rest::simple::SimpleRESTRecordResponse<SimpleUser>>(get_response.body).data.name);
    }
  }

  {
    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query"));
      EXPECT_EQ(404, static_cast<int>(cqs_response.code));
      EXPECT_EQ("{\"success\":false,\"message\":\"CQS handler not specified.\",\"error\":null}\n", cqs_response.body);
    }

    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list"));
      EXPECT_EQ(404, static_cast<int>(cqs_response.code));
      EXPECT_EQ("{\"success\":false,\"message\":\"CQS handler not found.\",\"error\":null}\n", cqs_response.body);
    }

    storage_http_interface.template AddCQSQuery<CQSQuery>("list");

    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list"));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("http://unittest.current.ai = DK,MZ", cqs_response.body);
    }
    {
      const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("grisha", "GN")));
      EXPECT_EQ(201, static_cast<int>(post_response.code));
    }
    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list"));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("http://unittest.current.ai = DK,GN,MZ", cqs_response.body);
    }
    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list?reverse_sort=false"));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("http://unittest.current.ai = DK,GN,MZ", cqs_response.body);
    }
    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list?reverse_sort=true"));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("http://unittest.current.ai = MZ,GN,DK", cqs_response.body);
    }
    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list?test_native_exception"));
      EXPECT_EQ(500, static_cast<int>(cqs_response.code));
      EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", cqs_response.body);
    }
    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list?test_current_exception"));
      EXPECT_EQ(500, static_cast<int>(cqs_response.code));
      EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", cqs_response.body);

      {
        const auto cqs_response = HTTP(POST(base_url + "/api/cqs/query/list", "<irrelevant POST body>"));
        EXPECT_EQ(405, static_cast<int>(cqs_response.code));
        EXPECT_EQ(
            "{\"success\":false,"
            "\"message\":null,"
            "\"error\":{"
            "\"name\":\"MethodNotAllowed\","
            "\"message\":\"CQS queries must be GET-s.\","
            "\"details\":{\"requested_method\":\"POST\"}}}\n",
            cqs_response.body);
      }

      // Duplicate query registration is not allowed.
      try {
        storage_http_interface.template AddCQSQuery<CQSQuery>("list");
        ASSERT_TRUE(false);
      } catch (const current::Exception& e) {
        EXPECT_EQ("RESTfulStorage::AddCQSQuery(), `list` is already registered.", e.OriginalDescription());
      }
    }

    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command", CQSCommand()));
      EXPECT_EQ(404, static_cast<int>(cqs_response.code));
      EXPECT_EQ("{\"success\":false,\"message\":\"CQS handler not specified.\",\"error\":null}\n", cqs_response.body);
    }

    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add", CQSCommand()));
      EXPECT_EQ(404, static_cast<int>(cqs_response.code));
      EXPECT_EQ("{\"success\":false,\"message\":\"CQS handler not found.\",\"error\":null}\n", cqs_response.body);
    }

    storage_http_interface.template AddCQSCommand<CQSCommand>("add");

    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add", CQSCommand({"alice", "bob"})));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("{\"url\":\"http://unittest.current.ai\",\"before\":3,\"after\":5}\n", cqs_response.body);
    }

    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list"));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("http://unittest.current.ai = DK,GN,MZ,alice,bob", cqs_response.body);
    }

    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add?test_native_exception&users=[]", ""));
      EXPECT_EQ(500, static_cast<int>(cqs_response.code));
      EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", cqs_response.body);
    }
    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add?test_current_exception&users=[]", ""));
      EXPECT_EQ(500, static_cast<int>(cqs_response.code));
      EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", cqs_response.body);
    }
    {
      const auto cqs_response =
          HTTP(POST(base_url + "/api/cqs/command/add?test_simple_rollback&users=[\"foo\",\"bar\",\"baz\"]", ""));
      EXPECT_EQ(500, static_cast<int>(cqs_response.code));
      EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", cqs_response.body);
    }
    {
      // As the CQS command adding "foo", "bar", and "baz" has been rolled back, they should not be on the list.
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/query/list"));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("http://unittest.current.ai = DK,GN,MZ,alice,bob", cqs_response.body);
    }
    {
      CQSCommand command({"this", "shall", "not", "pass"});
      command.test_rollback_with_json = true;
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add", command));
      EXPECT_EQ(412, static_cast<int>(cqs_response.code));
      EXPECT_EQ("{\"rolled_back\":true,\"command\":[\"this\",\"shall\",\"not\",\"pass\"]}\n", cqs_response.body);
    }
    {
      CQSCommand command({"this", "shall", "not", "pass"});
      command.test_rollback_with_raw_response = true;
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add", command));
      EXPECT_EQ(200, static_cast<int>(cqs_response.code));
      EXPECT_EQ("HA!", cqs_response.body);
    }

    {
      const auto cqs_response = HTTP(GET(base_url + "/api/cqs/command/add"));
      EXPECT_EQ(405, static_cast<int>(cqs_response.code));
      EXPECT_EQ(
          "{\"success\":false,"
          "\"message\":null,"
          "\"error\":{"
          "\"name\":\"MethodNotAllowed\","
          "\"message\":\"CQS commands must be {POST|PUT|PATCH}-es.\","
          "\"details\":{\"requested_method\":\"GET\"}}}\n",
          cqs_response.body);
    }

    // Test strict URL parsing, `users` is required and missing.
    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add", ""));
      EXPECT_EQ(400, static_cast<int>(cqs_response.code));
      const auto response = ParseJSON<generic::RESTGenericResponse>(cqs_response.body);
      EXPECT_FALSE(response.success);
      ASSERT_TRUE(Exists(response.message));
      EXPECT_EQ("CQS command or query URL parameters parse error.", Value(response.message));
    }

    // Test strict HTTP BODY parsing, `users` is required and missing.
    {
      const auto cqs_response = HTTP(POST(base_url + "/api/cqs/command/add", "{}"));
      EXPECT_EQ(400, static_cast<int>(cqs_response.code));
      const auto response = ParseJSON<generic::RESTGenericResponse>(cqs_response.body);
      EXPECT_FALSE(response.success);
      ASSERT_TRUE(Exists(response.message));
      EXPECT_EQ("CQS command or query HTTP body JSON parse error.", Value(response.message));
    }

    // Duplicate command registration is not allowed.
    try {
      storage_http_interface.template AddCQSCommand<CQSCommand>("add");
      ASSERT_TRUE(false);
    } catch (const current::Exception& e) {
      EXPECT_EQ("RESTfulStorage::AddCQSCommand(), `add` is already registered.", e.OriginalDescription());
    }
  }

  // TODO(dkorolev): Add and test per-language endpoints (F# format).
  // TODO(dkorolev): Also test transaction meta fields.
}

#ifndef STORAGE_ONLY_RUN_RESTFUL_TESTS

TEST(TransactionalStorage, RESTfulAPIDoesNotExposeHiddenFieldsTest) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using namespace current::storage::rest;

  using Storage1 = SimpleStorage<StreamInMemoryStreamPersister>;
  using Storage2 = PartiallyExposedStorage<StreamInMemoryStreamPersister>;

  EXPECT_EQ(6u, Storage1::FIELDS_COUNT);
  EXPECT_EQ(6u, Storage2::FIELDS_COUNT);

  auto storage1 = Storage1::CreateMasterStorage();
  auto storage2 = Storage2::CreateMasterStorage();

  static_assert(current::storage::rest::FieldExposedViaREST<Storage1, SimpleUserPersisted>::exposed, "");
  static_assert(current::storage::rest::FieldExposedViaREST<Storage1, SimplePostPersisted>::exposed, "");

  static_assert(current::storage::rest::FieldExposedViaREST<Storage2, SimpleUserPersistedExposed>::exposed, "");
  static_assert(!current::storage::rest::FieldExposedViaREST<Storage2, SimplePostPersistedNotExposed>::exposed, "");

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  const auto base_url = current::strings::Printf("http://localhost:%d", port);

  auto rest1 = RESTfulStorage<Storage1, current::storage::rest::Hypermedia>(
      *storage1, port, "/api1", "http://unittest.current.ai/api1");
  auto rest2 = RESTfulStorage<Storage2, current::storage::rest::Hypermedia>(
      *storage2, port, "/api2", "http://unittest.current.ai/api2");

  const auto fields1 = ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api1")).body);
  const auto fields2 = ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api2")).body);

  EXPECT_TRUE(fields1.url_data.count("user") == 1);
  EXPECT_TRUE(fields1.url_data.count("post") == 1);
  EXPECT_TRUE(fields1.url_data.count("like") == 1);
  EXPECT_EQ(6u, fields1.url_data.size());

  EXPECT_TRUE(fields2.url_data.count("user") == 1);
  EXPECT_TRUE(fields2.url_data.count("post") == 0);
  EXPECT_TRUE(fields2.url_data.count("like") == 1);
  EXPECT_EQ(5u, fields2.url_data.size());

  // Confirms the status returns proper URL prefix.
  EXPECT_EQ("http://unittest.current.ai/api1", fields1.url);
  EXPECT_EQ("http://unittest.current.ai/api1/status", fields1.url_status);

  EXPECT_EQ("http://unittest.current.ai/api2", fields2.url);
  EXPECT_EQ("http://unittest.current.ai/api2/status", fields2.url_status);
}

TEST(TransactionalStorage, ShuttingDownAPIReportsUpAsFalse) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using namespace current::storage::rest;
  using storage_t = SimpleStorage<StreamInMemoryStreamPersister>;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  auto storage = storage_t::CreateMasterStorage();

  const auto base_url = current::strings::Printf("http://localhost:%d", port);

  auto rest = RESTfulStorage<storage_t, current::storage::rest::Hypermedia>(
      *storage, port, "/api", "http://unittest.current.ai");

  EXPECT_TRUE(ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api")).body).up);
  EXPECT_TRUE(ParseJSON<HypermediaRESTStatus>(HTTP(GET(base_url + "/api/status")).body).up);
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(base_url + "/api/data/post/foo")).code));

  rest.SwitchHTTPEndpointsTo503s();

  EXPECT_FALSE(ParseJSON<HypermediaRESTTopLevel>(HTTP(GET(base_url + "/api")).body).up);
  EXPECT_FALSE(ParseJSON<HypermediaRESTStatus>(HTTP(GET(base_url + "/api/status")).body).up);
  EXPECT_EQ(503, static_cast<int>(HTTP(GET(base_url + "/api/data/post/foo")).code));
}

#ifdef CURRENT_STORAGE_PATCH_SUPPORT

namespace transactional_storage_test {

CURRENT_STRUCT(NonPatchableX) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(x, int32_t);
  CURRENT_CONSTRUCTOR(NonPatchableX)(std::string key = "", int32_t x = 0) : key(std::move(key)), x(x) {}
};

CURRENT_STRUCT(PatchToY) {
  CURRENT_FIELD(dy, int32_t, 0);
  CURRENT_CONSTRUCTOR(PatchToY)(int32_t dy = 0) : dy(dy) {}

  // To test the forwarding constructor.
  CURRENT_CONSTRUCTOR(PatchToY)(int32_t dy1, int32_t dy2, int32_t dy3) : dy(dy1 + dy2 + dy3) {}
};

CURRENT_STRUCT(PatchableY) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(y, int32_t);
  CURRENT_CONSTRUCTOR(PatchableY)(std::string key = "", int32_t y = 0) : key(std::move(key)), y(y) {}

  using patch_object_t = PatchToY;
  void PatchWith(const patch_object_t& patch) { y += patch.dy; }
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, NonPatchableX, NonPatchableXDictionary);
CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, PatchableY, PatchableYDictionary);

CURRENT_STORAGE(PatchTestStorage) {
  CURRENT_STORAGE_FIELD(x, NonPatchableXDictionary);
  CURRENT_STORAGE_FIELD(y, PatchableYDictionary);
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, PatchMutation) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  static_assert(!current::HasPatch<NonPatchableX>(), "");
  static_assert(current::HasPatch<PatchableY>(), "");

  using storage_t = PatchTestStorage<StreamInMemoryStreamPersister>;

  static_assert(std::is_same_v<typename storage_t::transaction_t, typename storage_t::stream_t::entry_t>, "");
  // clang-format off
  EXPECT_STREQ("Transaction<Variant<NonPatchableXDictionaryUpdated, PatchableYDictionaryUpdated, NonPatchableXDictionaryDeleted, PatchableYDictionaryDeleted, PatchableYDictionaryPatched>>",
               current::reflection::CurrentTypeName<typename storage_t::transaction_t>());
  // clang-format on

  EXPECT_EQ(2u, storage_t::FIELDS_COUNT);

  current::Owned<typename storage_t::stream_t> stream = storage_t::stream_t::CreateStream();
  current::Owned<storage_t> storage = storage_t::CreateMasterStorageAtopExistingStream(stream);

  EXPECT_TRUE(storage->IsMasterStorage());

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              EXPECT_FALSE(fields.x.Has("a"));
                              fields.x.Add(NonPatchableX{"a", 1});
                              EXPECT_TRUE(fields.x.Has("a"));
                              EXPECT_TRUE(Exists(fields.x["a"]));
                              EXPECT_EQ(1, Value(fields.x["a"]).x);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(1), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              EXPECT_FALSE(fields.y.Has("b"));
                              fields.y.Add(PatchableY{"b", 2});
                              ASSERT_TRUE(fields.y.Has("b"));
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(2, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(2), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              EXPECT_TRUE(fields.y.Patch("b", PatchToY(+10)));
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(12, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(3), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(12, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
  }

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              EXPECT_TRUE(fields.y.Patch("b", +100));
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(112, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(4), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(112, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
  }

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              EXPECT_TRUE(fields.y.Patch("b", +333, +333, +334));
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(1112, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(5), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              const auto b = Value(fields.y["b"]);
                              EXPECT_TRUE(fields.y.Patch(b, PatchToY(+1)));
                              EXPECT_TRUE(fields.y.Patch(b, +2));
                              EXPECT_TRUE(fields.y.Patch(b, 0, -1, -2));
                              EXPECT_EQ(1112, Value(fields.y["b"]).y);

                              EXPECT_FALSE(fields.y.Patch("no such key", -1));
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(6), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(1112, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
  }

  {
    const auto result = storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              EXPECT_TRUE(fields.y.Patch("b", PatchToY(+1000000)));
                              CURRENT_STORAGE_THROW_ROLLBACK();
                            })
                            .Go();
    EXPECT_FALSE(WasCommitted(result));
    EXPECT_EQ(static_cast<uint64_t>(6), stream->Data()->Size());
  }

  {
    const auto result = storage
                            ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.y["b"]));
                              EXPECT_EQ(1112, Value(fields.y["b"]).y);
                            })
                            .Go();

    EXPECT_TRUE(WasCommitted(result));
  }

  {
    std::vector<std::string> entries;
    for (const auto& entry : stream->Data()->Iterate()) {
      for (const auto& mutation : entry.entry.mutations) {
        entries.push_back(JSON<JSONFormat::Minimalistic>(mutation));
      }
    }
    ASSERT_EQ(8u, entries.size());
    // clang-format off
    EXPECT_EQ("{\"NonPatchableXDictionaryUpdated\":{\"us\":1,\"data\":{\"key\":\"a\",\"x\":1}}}", entries[0]);
    EXPECT_EQ("{\"PatchableYDictionaryUpdated\":{\"us\":5,\"data\":{\"key\":\"b\",\"y\":2}}}", entries[1]);
    EXPECT_EQ("{\"PatchableYDictionaryPatched\":{\"us\":9,\"key\":\"b\",\"patch\":{\"dy\":10}}}", entries[2]);
    EXPECT_EQ("{\"PatchableYDictionaryPatched\":{\"us\":13,\"key\":\"b\",\"patch\":{\"dy\":100}}}", entries[3]);
    EXPECT_EQ("{\"PatchableYDictionaryPatched\":{\"us\":17,\"key\":\"b\",\"patch\":{\"dy\":1000}}}", entries[4]);
    EXPECT_EQ("{\"PatchableYDictionaryPatched\":{\"us\":21,\"key\":\"b\",\"patch\":{\"dy\":1}}}", entries[5]);
    EXPECT_EQ("{\"PatchableYDictionaryPatched\":{\"us\":22,\"key\":\"b\",\"patch\":{\"dy\":2}}}", entries[6]);
    EXPECT_EQ("{\"PatchableYDictionaryPatched\":{\"us\":23,\"key\":\"b\",\"patch\":{\"dy\":-3}}}", entries[7]);
    // clang-format on
  }

  {
    current::Owned<storage_t> following_storage = storage_t::CreateFollowingStorageAtopExistingStream(stream);
    EXPECT_FALSE(following_storage->IsMasterStorage());
    EXPECT_EQ(6u, following_storage->UnderlyingStream()->Data()->Size());

    while (following_storage->LastAppliedTimestamp() < storage->LastAppliedTimestamp()) {
      std::this_thread::yield();
    }

    {
      const auto result = following_storage
                              ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                                ASSERT_TRUE(Exists(fields.x["a"]));
                                EXPECT_EQ(1, Value(fields.x["a"]).x);
                                ASSERT_TRUE(Exists(fields.y["b"]));
                                EXPECT_EQ(1112, Value(fields.y["b"]).y);
                              })
                              .Go();

      EXPECT_TRUE(WasCommitted(result));
    }
  }
}

#endif  // CURRENT_STORAGE_PATCH_SUPPORT

#endif  // STORAGE_ONLY_RUN_RESTFUL_TESTS
