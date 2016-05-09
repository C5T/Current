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

// TODO(dkorolev): Test dependency resolution to codenames.
// TODO(dkorolev): Test runtime status.

#define CURRENT_MOCK_TIME
#define EXTRA_KARL_LOGGING  // Make sure the schema dump part compiles. -- D.K.

#include "current_build.h"
#include "karl.h"

#include "test_service/generator.h"
#include "test_service/is_prime.h"
#include "test_service/annotator.h"
#include "test_service/filter.h"

#include "../Blocks/HTTP/api.h"

#include "../Sherlock/sherlock.h"

#include "../Bricks/strings/printf.h"

#include "../Bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_uint16(karl_test_port, PickPortForUnitTest(), "Local test port for the `Karl` master service.");
DEFINE_uint16(karl_generator_test_port, PickPortForUnitTest(), "Local test port for the `generator` service.");
DEFINE_uint16(karl_is_prime_test_port, PickPortForUnitTest(), "Local test port for the `is_prime` service.");
DEFINE_uint16(karl_annotator_test_port, PickPortForUnitTest(), "Local test port for the `annotator` service.");
DEFINE_uint16(karl_filter_test_port, PickPortForUnitTest(), "Local test port for the `filter` service.");
DEFINE_uint16(karl_nginx_port,
              PickPortForUnitTest(),
              "Local port for Nginx to listen for proxying requests to Claires.");
DEFINE_string(karl_nginx_config_file,
              "",
              "If set, run tests with Nginx assuming this file is included in the main Nginx config.");

#ifndef CURRENT_WINDOWS
DEFINE_string(karl_test_stream_persistence_file, ".current/stream", "Local file to store Karl's keepalives.");
DEFINE_string(karl_test_storage_persistence_file, ".current/storage", "Local file to store Karl's status.");
#else
#error "Sorry bro, you're out of luck. Karl works fine though; we checked. Ah, TODO(dkorolev), of course."
#endif

DEFINE_bool(karl_run_test_forever, false, "Set to `true` to run the Karl test forever.");

using unittest_karl_t =
    current::karl::GenericKarl<current::karl::default_user_status::status, karl_unittest::is_prime>;
using unittest_karl_status_t = typename unittest_karl_t::karl_status_t;

TEST(Karl, SmokeGenerator) {
  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  const unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);

  {
    const auto fields = current::strings::Split(
        HTTP(GET(Printf("http://localhost:%d/numbers?i=100&n=1", FLAGS_karl_generator_test_port))).body, '\t');
    ASSERT_EQ(2u, fields.size());
    EXPECT_EQ(100u, ParseJSON<idxts_t>(fields[0]).index);
    EXPECT_EQ("{\"x\":100,\"is_prime\":null}\n", fields[1]);
  }

  {
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(
        status = ParseJSON<current::karl::ClaireStatus>(
            HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_generator_test_port))).body));
    EXPECT_EQ("generator", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_port))).body));
    EXPECT_EQ(1u, status.size()) << JSON(status);
    ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    ASSERT_TRUE(per_ip_services.count(generator.ClaireCodename())) << JSON(per_ip_services);
    auto& per_codename = per_ip_services[generator.ClaireCodename()];
    // As a lot of "time" has passed since the "last" keepalive, `up` would actually be `false`. Don't test it.
    EXPECT_EQ("generator", per_codename.service);
  }
}

TEST(Karl, SmokeIsPrime) {
  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  const unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  EXPECT_EQ("YES\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=2", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=2017", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=1000000007", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=-1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=0", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=10", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=1369", FLAGS_karl_is_prime_test_port))).body);

  {
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_is_prime_test_port))).body));
    EXPECT_EQ("is_prime", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_port))).body));
    EXPECT_EQ(1u, status.size()) << JSON(status);
    ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    ASSERT_TRUE(per_ip_services.count(is_prime.ClaireCodename())) << JSON(per_ip_services);
    auto& per_codename = per_ip_services[is_prime.ClaireCodename()];
    EXPECT_TRUE(Exists<current::karl::current_service_state::up>(per_codename.currently));
    EXPECT_EQ("is_prime", per_codename.service);
  }
}

TEST(Karl, SmokeAnnotator) {
  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  const unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  const karl_unittest::ServiceAnnotator annotator(FLAGS_karl_annotator_test_port,
                                                  Printf("http://localhost:%d", FLAGS_karl_generator_test_port),
                                                  Printf("http://localhost:%d", FLAGS_karl_is_prime_test_port),
                                                  karl_locator);
  ASSERT_NO_THROW(const auto x37 = ParseJSON<karl_unittest::Number>(
                      current::strings::Split(HTTP(GET(Printf("http://localhost:%d/annotated?i=37&n=1",
                                                              FLAGS_karl_annotator_test_port))).body,
                                              '\t').back());
                  EXPECT_EQ(37, x37.x);
                  ASSERT_TRUE(Exists(x37.is_prime));
                  EXPECT_TRUE(Value(x37.is_prime)););
  ASSERT_NO_THROW(const auto x39 = ParseJSON<karl_unittest::Number>(
                      current::strings::Split(HTTP(GET(Printf("http://localhost:%d/annotated?i=39&n=1",
                                                              FLAGS_karl_annotator_test_port))).body,
                                              '\t').back());
                  EXPECT_EQ(39, x39.x);
                  ASSERT_TRUE(Exists(x39.is_prime));
                  EXPECT_FALSE(Value(x39.is_prime)););

  {
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(
        status = ParseJSON<current::karl::ClaireStatus>(
            HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_annotator_test_port))).body));
    EXPECT_EQ("annotator", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_port))).body));

    EXPECT_EQ(1u, status.size()) << JSON(status);
    ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status["127.0.0.1"].services;
    EXPECT_EQ(3u, per_ip_services.size());
    EXPECT_EQ("annotator", per_ip_services[annotator.ClaireCodename()].service);
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
  }
}

TEST(Karl, SmokeFilter) {
  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  const unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  const karl_unittest::ServiceAnnotator annotator(FLAGS_karl_annotator_test_port,
                                                  Printf("http://localhost:%d", FLAGS_karl_generator_test_port),
                                                  Printf("http://localhost:%d", FLAGS_karl_is_prime_test_port),
                                                  karl_locator);
  const karl_unittest::ServiceFilter filter(
      FLAGS_karl_filter_test_port, Printf("http://localhost:%d", FLAGS_karl_annotator_test_port), karl_locator);

  ASSERT_NO_THROW(
      // 10-th (index=9 for 0-based) prime is `29`.
      const auto x29 = ParseJSON<karl_unittest::Number>(
          current::strings::Split(
              HTTP(GET(Printf("http://localhost:%d/primes?i=9&n=1", FLAGS_karl_filter_test_port))).body, '\t')
              .back());
      EXPECT_EQ(29, x29.x);
      ASSERT_TRUE(Exists(x29.is_prime));
      EXPECT_TRUE(Value(x29.is_prime)));
  ASSERT_NO_THROW(
      // 20-th (index=19 for 0-based) prime is `71`.
      const auto x71 = ParseJSON<karl_unittest::Number>(
          current::strings::Split(
              HTTP(GET(Printf("http://localhost:%d/primes?i=19&n=1", FLAGS_karl_filter_test_port))).body, '\t')
              .back());
      EXPECT_EQ(71, x71.x);
      ASSERT_TRUE(Exists(x71.is_prime));
      EXPECT_TRUE(Value(x71.is_prime)););

  {
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_filter_test_port))).body));
    EXPECT_EQ("filter", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_port))).body));
    EXPECT_EQ(1u, status.size()) << JSON(status);
    ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status["127.0.0.1"].services;
    EXPECT_EQ(4u, per_ip_services.size());
    EXPECT_EQ("annotator", per_ip_services[annotator.ClaireCodename()].service);
    EXPECT_EQ("filter", per_ip_services[filter.ClaireCodename()].service);
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
  }
}

TEST(Karl, Deregister) {
  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  const unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));

  // No services registered.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
    EXPECT_TRUE(status.empty()) << JSON(status);
  }

  {
    const karl_unittest::ServiceGenerator generator(
        FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);
    // The `generator` service is registered.
    {
      unittest_karl_status_t status;
      ASSERT_NO_THROW(
          status = ParseJSON<unittest_karl_status_t>(
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
      EXPECT_EQ(1u, status.size()) << JSON(status);
      ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    }

    {
      const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
      // The `generator` and `is_prime` services are registered.
      {
        unittest_karl_status_t status;
        ASSERT_NO_THROW(
            status = ParseJSON<unittest_karl_status_t>(
                HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
        EXPECT_EQ(1u, status.size()) << JSON(status);
        ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
        auto& per_ip_services = status["127.0.0.1"].services;
        EXPECT_EQ(2u, per_ip_services.size());
        EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
        EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
      }
    }
    // The `generator` should be the only service registered.
    {
      unittest_karl_status_t status;
      ASSERT_NO_THROW(
          status = ParseJSON<unittest_karl_status_t>(
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
      EXPECT_EQ(1u, status.size()) << JSON(status);
      ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    }
  }

  // All services should be deregistered by this moment.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
    EXPECT_TRUE(status.empty()) << JSON(status);
  }
}

TEST(Karl, DeregisterWithNginx) {
  // Run the test only if `karl_nginx_config_file` flag is set.
  if (FLAGS_karl_nginx_config_file.empty()) {
    return;
  }

  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);

  const current::karl::KarlNginxParameters nginx_parameters(FLAGS_karl_nginx_port,
                                                            FLAGS_karl_nginx_config_file);
  const std::string karl_nginx_base_url = "http://localhost:" + current::ToString(FLAGS_karl_nginx_port);
  const unittest_karl_t karl(FLAGS_karl_test_port,
                             FLAGS_karl_test_stream_persistence_file,
                             FLAGS_karl_test_storage_persistence_file,
                             "/",
                             karl_nginx_base_url,
                             nginx_parameters);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));

  // No services registered.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
    EXPECT_TRUE(status.empty()) << JSON(status);
  }

  std::string generator_proxied_status_url;
  std::string is_prime_proxied_status_url;
  {
    const karl_unittest::ServiceGenerator generator(
        FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);
    // The `generator` service is registered.
    {
      unittest_karl_status_t status;
      ASSERT_NO_THROW(
          status = ParseJSON<unittest_karl_status_t>(
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
      EXPECT_EQ(1u, status.size()) << JSON(status);
      ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
      ASSERT_TRUE(Exists(per_ip_services[generator.ClaireCodename()].url_status_page_proxied));
      generator_proxied_status_url = Value(per_ip_services[generator.ClaireCodename()].url_status_page_proxied);
      EXPECT_EQ(karl_nginx_base_url + "/proxied/" + generator.ClaireCodename(), generator_proxied_status_url);
    }

    // Check that `generator`'s status page is accessible via Nginx.
    {
      current::karl::ClaireStatus status;
      // Must wait for Nginx config reload to take effect.
      while (true) {
        try {
          const auto response = HTTP(GET(generator_proxied_status_url));
          if (response.code == HTTPResponseCode.OK) {
            status = ParseJSON<current::karl::ClaireStatus>(response.body);
            break;
          }
        } catch (const current::net::NetworkException& e) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      EXPECT_EQ("generator", status.service);
    }

    {
      const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
      // The `generator` and `is_prime` services are registered.
      {
        unittest_karl_status_t status;
        ASSERT_NO_THROW(
            status = ParseJSON<unittest_karl_status_t>(
                HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
        EXPECT_EQ(1u, status.size()) << JSON(status);
        ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
        auto& per_ip_services = status["127.0.0.1"].services;
        EXPECT_EQ(2u, per_ip_services.size());
        EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
        EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
        ASSERT_TRUE(Exists(per_ip_services[is_prime.ClaireCodename()].url_status_page_proxied));
        is_prime_proxied_status_url = Value(per_ip_services[is_prime.ClaireCodename()].url_status_page_proxied);
        EXPECT_EQ(karl_nginx_base_url + "/proxied/" + is_prime.ClaireCodename(), is_prime_proxied_status_url);
      }
      // Check that `is_prime`'s status page is accessible via Nginx.
      {
        current::karl::ClaireStatus status;
        // Must wait for Nginx config reload to take effect.
        while (true) {
          try {
            const auto response = HTTP(GET(is_prime_proxied_status_url));
            if (response.code == HTTPResponseCode.OK) {
              status = ParseJSON<current::karl::ClaireStatus>(response.body);
              break;
            }
          } catch (const current::net::NetworkException& e) {
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        EXPECT_EQ("is_prime", status.service);
      }
    }

    // The `generator` should be the only service registered.
    {
      unittest_karl_status_t status;
      ASSERT_NO_THROW(
          status = ParseJSON<unittest_karl_status_t>(
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
      EXPECT_EQ(1u, status.size()) << JSON(status);
      ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    }
  }

  // All services should be deregistered by this moment.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_port))).body));
    EXPECT_TRUE(status.empty()) << JSON(status);
  }

  // Check that both proxied services are removed from Nginx config.
  {
    // Must wait for Nginx config reload to take effect.
    while (HTTP(GET(is_prime_proxied_status_url)).code != HTTPResponseCode.NotFound) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Spin lock.
    }
    while (HTTP(GET(generator_proxied_status_url)).code != HTTPResponseCode.NotFound) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Spin lock.
    }
  }
}

TEST(Karl, DisconnectedByTimout) {
  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));

  current::karl::ClaireStatus claire;
  claire.service = "unittest";
  claire.codename = "ABCDEF";
  claire.local_port = 8888;
  {
    const std::string keepalive_url = Printf("%s?codename=%s&port=%d",
                                             karl_locator.address_port_route.c_str(),
                                             claire.codename.c_str(),
                                             claire.local_port);
    const auto response = HTTP(POST(keepalive_url, claire));
    EXPECT_EQ(200, static_cast<int>(response.code));
    while (karl.ActiveServicesCount() == 0u) {
      ;  // Spin lock.
    }
    const auto result = karl.InternalExposeStorage()
                            .ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.claires[claire.codename]));
                              EXPECT_EQ(current::karl::ClaireRegisteredState::Active,
                                        Value(fields.claires[claire.codename]).registered_state);
                            })
                            .Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  current::time::SetNow(std::chrono::microseconds(100 * 1000 * 1000),
                        std::chrono::microseconds(101 * 1000 * 1000));
  bool is_timeouted_persisted = false;
  while (!is_timeouted_persisted) {
    is_timeouted_persisted =
        Value(karl.InternalExposeStorage()
                  .ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) -> bool {
                    EXPECT_TRUE(Exists(fields.claires[claire.codename]));
                    return Value(fields.claires[claire.codename]).registered_state ==
                           current::karl::ClaireRegisteredState::DisconnectedByTimeout;
                  })
                  .Go());
  }
}

TEST(Karl, DisconnectedByTimoutWithNginx) {
  // Run the test only if `karl_nginx_config_file` flag is set.
  if (FLAGS_karl_nginx_config_file.empty()) {
    return;
  }

  current::time::ResetToZero();

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);

  const current::karl::KarlNginxParameters nginx_parameters(FLAGS_karl_nginx_port,
                                                            FLAGS_karl_nginx_config_file);
  const std::string karl_nginx_base_url = "http://localhost:" + current::ToString(FLAGS_karl_nginx_port);
  unittest_karl_t karl(FLAGS_karl_test_port,
                       FLAGS_karl_test_stream_persistence_file,
                       FLAGS_karl_test_storage_persistence_file,
                       "/",
                       karl_nginx_base_url,
                       nginx_parameters);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));

  current::karl::ClaireStatus claire;
  claire.service = "unittest";
  claire.codename = "ABCDEF";
  claire.local_port = PickPortForUnitTest();
  // Register a fake service.
  HTTP(claire.local_port).Register("/.current", [](Request r) { r("GOTIT\n"); });

  {
    const std::string keepalive_url = Printf("%s?codename=%s&port=%d",
                                             karl_locator.address_port_route.c_str(),
                                             claire.codename.c_str(),
                                             claire.local_port);
    const auto response = HTTP(POST(keepalive_url, claire));
    EXPECT_EQ(200, static_cast<int>(response.code));
    while (karl.ActiveServicesCount() == 0u) {
      ;  // Spin lock.
    }
    const auto result = karl.InternalExposeStorage()
                            .ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) {
                              ASSERT_TRUE(Exists(fields.claires[claire.codename]));
                              EXPECT_EQ(current::karl::ClaireRegisteredState::Active,
                                        Value(fields.claires[claire.codename]).registered_state);
                            })
                            .Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  const std::string proxied_url =
      "http://localhost:" + current::ToString(FLAGS_karl_nginx_port) + "/proxied/" + claire.codename;
  {
    // Must wait for Nginx config reload to take effect.
    while (true) {
      try {
        const auto response = HTTP(GET(proxied_url));
        if (response.code == HTTPResponseCode.OK) {
          EXPECT_EQ("GOTIT\n", response.body);
          break;
        }
      } catch (const current::net::NetworkException& e) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  current::time::SetNow(std::chrono::microseconds(100 * 1000 * 1000),
                        std::chrono::microseconds(101 * 1000 * 1000));
  bool is_timeouted_persisted = false;
  while (!is_timeouted_persisted) {
    is_timeouted_persisted =
        Value(karl.InternalExposeStorage()
                  .ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) -> bool {
                    EXPECT_TRUE(Exists(fields.claires[claire.codename]));
                    return Value(fields.claires[claire.codename]).registered_state ==
                           current::karl::ClaireRegisteredState::DisconnectedByTimeout;
                  })
                  .Go());
  }

  // This is a bit flaky.
  {
    // Must wait for Nginx config reload to take effect.
    while (true) {
      try {
        const auto response = HTTP(GET(proxied_url));
        if (response.code == HTTPResponseCode.NotFound) {
          break;
        }
      } catch (const current::net::NetworkException& e) {
        break;
      }
    }
  }
}

// To run a `curl`-able test: ./.current/test --karl_run_test_forever --gtest_filter=Karl.EndToEndTest
TEST(Karl, EndToEndTest) {
  current::time::ResetToZero();

  if (FLAGS_karl_run_test_forever) {
    // Instructions:
    // * Generator, Annotator, Filter: Exposed as Sherlock streams; curl `?size`, `?i=$INDEX&n=$COUNT`.
    // * IsPrime: Exposing a single endpoint; curl `?x=42` or `?x=43` to test.
    // * Karl: Displaying status; curl ``.
    // TODO(dkorolev): Have Karl expose the DOT/SVG, over the desired period of time. And test it.
    std::cerr << "Karl      :: localhost:" << FLAGS_karl_test_port << '\n';
    std::cerr << "Generator :: localhost:" << FLAGS_karl_generator_test_port << "/numbers\n";
    std::cerr << "IsPrime   :: localhost:" << FLAGS_karl_is_prime_test_port << "/is_prime?x=42\n";
    std::cerr << "Annotator :: localhost:" << FLAGS_karl_annotator_test_port << "/annotated\n";
    std::cerr << "Filter    :: localhost:" << FLAGS_karl_filter_test_port << "/primes\n";
  }

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_test_storage_persistence_file);
  const unittest_karl_t karl(
      FLAGS_karl_test_port, FLAGS_karl_test_stream_persistence_file, FLAGS_karl_test_storage_persistence_file);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(FLAGS_karl_generator_test_port,
                                                  std::chrono::microseconds(10000),  // 100 per second.
                                                  karl_locator);
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  const karl_unittest::ServiceAnnotator annotator(FLAGS_karl_annotator_test_port,
                                                  Printf("http://localhost:%d", FLAGS_karl_generator_test_port),
                                                  Printf("http://localhost:%d", FLAGS_karl_is_prime_test_port),
                                                  karl_locator);
  const karl_unittest::ServiceFilter filter(
      FLAGS_karl_filter_test_port, Printf("http://localhost:%d", FLAGS_karl_annotator_test_port), karl_locator);

  {
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_filter_test_port))).body));
    EXPECT_EQ("filter", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_port))).body));
    EXPECT_EQ(1u, status.size()) << JSON(status);
    ASSERT_TRUE(status.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status["127.0.0.1"].services;
    EXPECT_EQ(4u, per_ip_services.size());
    EXPECT_EQ("annotator", per_ip_services[annotator.ClaireCodename()].service);
    EXPECT_EQ("filter", per_ip_services[filter.ClaireCodename()].service);
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
  }

  if (FLAGS_karl_run_test_forever) {
    std::cerr << "Running forever, CTRL+C to terminate.\n";
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}
