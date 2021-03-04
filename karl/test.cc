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

#define CURRENT_MOCK_TIME
#define EXTRA_KARL_LOGGING  // Make sure the schema dump part compiles. -- D.K.
#define CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS

#include "current_build.h"

#include "karl.h"

#include "test_service/generator.h"
#include "test_service/is_prime.h"
#include "test_service/annotator.h"
#include "test_service/filter.h"

#include "../blocks/http/api.h"

#include "../stream/stream.h"

#include "../bricks/strings/printf.h"

#include "../bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_uint16(karl_test_keepalives_port, PickPortForUnitTest(), "Local test port for the `Karl` keepalive listener.");
DEFINE_uint16(karl_test_fleet_view_port, PickPortForUnitTest(), "Local test port for the `Karl` fleet view listener.");
DEFINE_uint16(karl_generator_test_port, PickPortForUnitTest(), "Local test port for the `generator` service.");
DEFINE_uint16(karl_is_prime_test_port, PickPortForUnitTest(), "Local test port for the `is_prime` service.");
DEFINE_uint16(karl_annotator_test_port, PickPortForUnitTest(), "Local test port for the `annotator` service.");
DEFINE_uint16(karl_filter_test_port, PickPortForUnitTest(), "Local test port for the `filter` service.");
#ifndef CURRENT_CI
DEFINE_uint16(karl_nginx_port,
              PickPortForUnitTest(),
              "Local port for Nginx to listen for proxying requests to Claires.");

DEFINE_string(karl_nginx_config_file,
              "",
              "If set, run tests with Nginx assuming this file is included in the main Nginx config.");
#endif  // CURRENT_CI

#ifndef CURRENT_WINDOWS
DEFINE_string(karl_test_stream_persistence_file, ".current/stream", "Local file to store Karl's keepalives.");
DEFINE_string(karl_test_storage_persistence_file, ".current/storage", "Local file to store Karl's status.");
#else
DEFINE_string(karl_test_stream_persistence_file, "./stream.tmp", "Local file to store Karl's keepalives.");
DEFINE_string(karl_test_storage_persistence_file, "./storage.tmp", "Local file to store Karl's status.");
#endif

DEFINE_bool(karl_run_test_forever, false, "Set to `true` to run the Karl test forever.");

DEFINE_bool(karl_overwrite_golden_files, false, "Set to true to have SVG golden files created/overwritten.");

using unittest_karl_t = current::karl::GenericKarl<current::karl::UseOwnStorage,
                                                   current::karl::default_user_status::status,
                                                   karl_unittest::is_prime>;
using unittest_karl_status_t = typename unittest_karl_t::karl_status_t;

static current::karl::KarlParameters UnittestKarlParameters() {
  current::karl::KarlParameters params;
  params.keepalives_port = FLAGS_karl_test_keepalives_port;
  params.fleet_view_port = FLAGS_karl_test_fleet_view_port;
  params.stream_persistence_file = FLAGS_karl_test_stream_persistence_file;
  params.storage_persistence_file = FLAGS_karl_test_storage_persistence_file;
  return params;
}

TEST(Karl, SmokeGenerator) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
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
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_generator_test_port))).body));
    EXPECT_EQ("generator", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    ASSERT_TRUE(per_ip_services.count(generator.ClaireCodename())) << JSON(per_ip_services);
    auto& per_codename = per_ip_services[generator.ClaireCodename()];
    // As a lot of "time" has passed since the "last" keepalive, `up` would actually be `false`. Don't test it.
    EXPECT_EQ("generator", per_codename.service);
  }
}

TEST(Karl, SmokeIsPrime) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  EXPECT_EQ("YES\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=2", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=2017", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=1000000007", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=-1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=0", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=10", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=1369", FLAGS_karl_is_prime_test_port))).body);

  {
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_is_prime_test_port))).body));
    EXPECT_EQ("is_prime", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    ASSERT_TRUE(per_ip_services.count(is_prime.ClaireCodename())) << JSON(per_ip_services);
    auto& per_codename = per_ip_services[is_prime.ClaireCodename()];
    EXPECT_TRUE(Exists<current::karl::current_service_state::up>(per_codename.currently));
    EXPECT_EQ("is_prime", per_codename.service);
  }
}

TEST(Karl, SmokeAnnotator) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
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
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_annotator_test_port))).body));
    EXPECT_EQ("annotator", status.service);
  }

  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_fleet_view_port))).body));

    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(3u, per_ip_services.size());
    EXPECT_EQ("annotator", per_ip_services[annotator.ClaireCodename()].service);
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
  }
}

TEST(Karl, SmokeFilter) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
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
              HTTP(GET(Printf("http://localhost:%d/primes?i=9&n=1", FLAGS_karl_filter_test_port))).body, '\t').back());
      EXPECT_EQ(29, x29.x);
      ASSERT_TRUE(Exists(x29.is_prime));
      EXPECT_TRUE(Value(x29.is_prime)));
  ASSERT_NO_THROW(
      // 20-th (index=19 for 0-based) prime is `71`.
      const auto x71 = ParseJSON<karl_unittest::Number>(
          current::strings::Split(
              HTTP(GET(Printf("http://localhost:%d/primes?i=19&n=1", FLAGS_karl_filter_test_port))).body, '\t').back());
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
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(4u, per_ip_services.size());
    EXPECT_EQ("annotator", per_ip_services[annotator.ClaireCodename()].service);
    EXPECT_EQ("filter", per_ip_services[filter.ClaireCodename()].service);
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
  }
}

TEST(Karl, Deregister) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));

  // No services registered.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_TRUE(status.machines.empty()) << JSON(status);
  }

  {
    const karl_unittest::ServiceGenerator generator(
        FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);
    // The `generator` service is registered.
    {
      unittest_karl_status_t status;
      ASSERT_NO_THROW(
          status = ParseJSON<unittest_karl_status_t>(
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
      EXPECT_EQ(1u, status.machines.size()) << JSON(status);
      ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status.machines["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    }

    {
      const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
      // The `generator` and `is_prime` services are registered.
      {
        unittest_karl_status_t status;
        ASSERT_NO_THROW(
            status = ParseJSON<unittest_karl_status_t>(HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only",
                                                                       FLAGS_karl_test_fleet_view_port))).body));
        EXPECT_EQ(1u, status.machines.size()) << JSON(status);
        ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
        auto& per_ip_services = status.machines["127.0.0.1"].services;
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
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
      EXPECT_EQ(1u, status.machines.size()) << JSON(status);
      ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status.machines["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    }
  }

  // All services should be deregistered by this moment.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_TRUE(status.machines.empty()) << JSON(status);
  }
}

#ifndef CURRENT_CI
TEST(Karl, DeregisterWithNginx) {
  // Run the test only if `karl_nginx_config_file` flag is set.
  if (FLAGS_karl_nginx_config_file.empty()) {
    return;
  }

  current::time::ResetToZero();

  auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);

  unittest_karl_t karl(params.SetNginxParameters(
      current::karl::KarlNginxParameters(FLAGS_karl_nginx_port, FLAGS_karl_nginx_config_file)));
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));

  const std::string karl_nginx_base_url = "http://localhost:" + current::ToString(FLAGS_karl_nginx_port);

  // No services registered.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_TRUE(status.machines.empty()) << JSON(status);
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
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
      EXPECT_EQ(1u, status.machines.size()) << JSON(status);
      ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status.machines["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
      ASSERT_TRUE(Exists(per_ip_services[generator.ClaireCodename()].url_status_page_proxied)) << JSON(per_ip_services);
      generator_proxied_status_url = Value(per_ip_services[generator.ClaireCodename()].url_status_page_proxied);
      EXPECT_EQ(karl_nginx_base_url + "/live/" + generator.ClaireCodename(), generator_proxied_status_url);
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
        } catch (const current::net::NetworkException&) {
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
            status = ParseJSON<unittest_karl_status_t>(HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only",
                                                                       FLAGS_karl_test_fleet_view_port))).body));
        EXPECT_EQ(1u, status.machines.size()) << JSON(status);
        ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
        auto& per_ip_services = status.machines["127.0.0.1"].services;
        EXPECT_EQ(2u, per_ip_services.size());
        EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
        EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
        ASSERT_TRUE(Exists(per_ip_services[is_prime.ClaireCodename()].url_status_page_proxied))
            << JSON(per_ip_services);
        is_prime_proxied_status_url = Value(per_ip_services[is_prime.ClaireCodename()].url_status_page_proxied);
        EXPECT_EQ(karl_nginx_base_url + "/live/" + is_prime.ClaireCodename(), is_prime_proxied_status_url);
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
          } catch (const current::net::NetworkException&) {
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
              HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
      EXPECT_EQ(1u, status.machines.size()) << JSON(status);
      ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
      auto& per_ip_services = status.machines["127.0.0.1"].services;
      EXPECT_EQ(1u, per_ip_services.size());
      EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    }
  }

  // All services should be deregistered by this moment.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_TRUE(status.machines.empty()) << JSON(status);
  }

  // Check that both proxied services are removed from Nginx config.
  {
    // Must wait for Nginx config reload to take effect.
    while (HTTP(GET(is_prime_proxied_status_url)).code != HTTPResponseCode.NotFound) {
      // Spin lock, exprerimentally confirmed to be better than `std::this_thread::yield()` here.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (HTTP(GET(generator_proxied_status_url)).code != HTTPResponseCode.NotFound) {
      // Spin lock, exprerimentally confirmed to be better than `std::this_thread::yield()` here.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}
#endif  // CURRENT_CI

#ifndef CURRENT_CI_TRAVIS  // NOTE(dkorolev): Disable Karl test on Travis, as it's overloaded and they often time out.

TEST(Karl, DisconnectedByTimout) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));

  current::karl::ClaireStatus claire;
  claire.service = "unittest";
  claire.codename = "ABCDEF";
  claire.local_port = 8888;
  {
    const std::string keepalive_url = Printf(
        "%s?codename=%s&port=%d", karl_locator.address_port_route.c_str(), claire.codename.c_str(), claire.local_port);
    const auto response = HTTP(POST(keepalive_url, claire));
    EXPECT_EQ(200, static_cast<int>(response.code));
    while (karl.ActiveServicesCount() == 0u) {
      std::this_thread::yield();
    }
    const auto result =
        karl.BorrowStorage()->ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) {
          ASSERT_TRUE(Exists(fields.claires[claire.codename]));
          EXPECT_EQ(current::karl::ClaireRegisteredState::Active,
                    Value(fields.claires[claire.codename]).registered_state);
        }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  current::time::SetNow(std::chrono::microseconds(100 * 1000 * 1000), std::chrono::microseconds(101 * 1000 * 1000));
  bool is_timeouted_persisted = false;
  while (!is_timeouted_persisted) {
    is_timeouted_persisted = Value(
        karl.BorrowStorage()->ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) -> bool {
          EXPECT_TRUE(Exists(fields.claires[claire.codename]));
          return Value(fields.claires[claire.codename]).registered_state ==
                 current::karl::ClaireRegisteredState::DisconnectedByTimeout;
        }).Go());
  }
}

#ifndef CURRENT_CI
TEST(Karl, DisconnectedByTimoutWithNginx) {
  // Run the test only if `karl_nginx_config_file` flag is set.
  if (FLAGS_karl_nginx_config_file.empty()) {
    return;
  }

  current::time::ResetToZero();

  auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);

  const unittest_karl_t karl(params.SetNginxParameters(
      current::karl::KarlNginxParameters(FLAGS_karl_nginx_port, FLAGS_karl_nginx_config_file)));
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));

  const std::string karl_nginx_base_url = "http://localhost:" + current::ToString(FLAGS_karl_nginx_port);

  current::karl::ClaireStatus claire;
  claire.service = "unittest";
  claire.codename = "ABCDEF";
  claire.local_port = PickPortForUnitTest();
  // Register a fake service.
  auto http_scope = HTTP(claire.local_port).Register("/.current", [](Request r) { r("GOTIT\n"); });

  {
    const std::string keepalive_url = Printf(
        "%s?codename=%s&port=%d", karl_locator.address_port_route.c_str(), claire.codename.c_str(), claire.local_port);
    const auto response = HTTP(POST(keepalive_url, claire));
    EXPECT_EQ(200, static_cast<int>(response.code));
    while (karl.ActiveServicesCount() == 0u) {
      std::this_thread::yield();
    }
    const auto result =
        karl.BorrowStorage()->ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) {
          ASSERT_TRUE(Exists(fields.claires[claire.codename]));
          EXPECT_EQ(current::karl::ClaireRegisteredState::Active,
                    Value(fields.claires[claire.codename]).registered_state);
        }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  const std::string proxied_url =
      "http://localhost:" + current::ToString(FLAGS_karl_nginx_port) + "/live/" + claire.codename;
  {
    // Must wait for Nginx config reload to take effect.
    while (true) {
      try {
        const auto response = HTTP(GET(proxied_url));
        if (response.code == HTTPResponseCode.OK) {
          EXPECT_EQ("GOTIT\n", response.body);
          break;
        }
      } catch (const current::net::NetworkException&) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  current::time::SetNow(std::chrono::microseconds(100 * 1000 * 1000), std::chrono::microseconds(101 * 1000 * 1000));
  bool is_timeouted_persisted = false;
  while (!is_timeouted_persisted) {
    is_timeouted_persisted = Value(
        karl.BorrowStorage()->ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) -> bool {
          EXPECT_TRUE(Exists(fields.claires[claire.codename]));
          return Value(fields.claires[claire.codename]).registered_state ==
                 current::karl::ClaireRegisteredState::DisconnectedByTimeout;
        }).Go());
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
      } catch (const current::net::NetworkException&) {
        break;
      }
    }
  }
}
#endif  // CURRENT_CI

TEST(Karl, ChangeKarlWhichClaireReportsTo) {
  current::time::ResetToZero();

  // Start primary `Karl`.
  const auto params = UnittestKarlParameters();
  const auto primary_stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto primary_storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t primary_karl(params);
  const current::karl::Locator primary_karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));

  // Start secondary `Karl`.
  current::karl::KarlParameters secondary_karl_params;
  secondary_karl_params.keepalives_port = PickPortForUnitTest();
  secondary_karl_params.fleet_view_port = PickPortForUnitTest();
  secondary_karl_params.stream_persistence_file = params.stream_persistence_file + "_secondary";
  secondary_karl_params.storage_persistence_file = params.storage_persistence_file + "_secondary";
  const auto secondary_stream_file_remover =
      current::FileSystem::ScopedRmFile(secondary_karl_params.stream_persistence_file);
  const auto secondary_storage_file_remover =
      current::FileSystem::ScopedRmFile(secondary_karl_params.storage_persistence_file);
  const unittest_karl_t secondary_karl(secondary_karl_params);
  const current::karl::Locator secondary_karl_locator(
      Printf("http://localhost:%d/", secondary_karl_params.keepalives_port));

  // No services registered in both `Karl`s.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_TRUE(status.machines.empty()) << JSON(status);
  }
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only",
                                                                   secondary_karl_params.fleet_view_port))).body));
    EXPECT_TRUE(status.machines.empty()) << JSON(status);
  }

  karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), primary_karl_locator);
  // The `generator` service is registered in the primary `Karl`.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
  }

  // Make `generator` report to the `secondary_karl`.
  {
    const std::string report_to_url = Printf("http://localhost:%d/.current?report_to=%s",
                                             FLAGS_karl_generator_test_port,
                                             secondary_karl_locator.address_port_route.c_str());
    const auto response = HTTP(POST(report_to_url, ""));
    EXPECT_EQ(200, static_cast<int>(response.code));
    const std::string expected_body =
        Printf("Now reporting to '%s'.\n", secondary_karl_locator.address_port_route.c_str());
    EXPECT_EQ(expected_body, response.body);
    EXPECT_EQ(secondary_karl_locator.address_port_route, generator.Claire().GetKarlLocator().address_port_route);
  }

  // Check that `generator`'s Claire reports new Karl URL.
  {
    const std::string generator_claire_url = Printf("http://localhost:%d/.current", FLAGS_karl_generator_test_port);
    const auto response = HTTP(GET(generator_claire_url));
    EXPECT_EQ(200, static_cast<int>(response.code));
    current::karl::ClaireStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatus>(response.body));
    EXPECT_EQ(secondary_karl_locator.address_port_route, status.reporting_to);
  }

  while (secondary_karl.ActiveServicesCount() == 0u) {
    std::this_thread::yield();
  }
  // The `generator` service is registered in the secondary `Karl`.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only",
                                                                   secondary_karl_params.fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
  }

  current::time::SetNow(std::chrono::microseconds(100 * 1000 * 1000), std::chrono::microseconds(101 * 1000 * 1000));
  // The primary `Karl` marks the `generator` service as `timeouted`.
  {
    bool is_timeouted_persisted = false;
    while (!is_timeouted_persisted) {
      is_timeouted_persisted =
          Value(primary_karl.BorrowStorage()
                    ->ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) -> bool {
                      EXPECT_TRUE(Exists(fields.claires[generator.ClaireCodename()]));
                      return Value(fields.claires[generator.ClaireCodename()]).registered_state ==
                             current::karl::ClaireRegisteredState::DisconnectedByTimeout;
                    })
                    .Go());
    }
  }

  // Make `generator` report to the `primary_karl`.
  {
    generator.Claire().SetKarlLocator(primary_karl_locator, current::karl::ForceSendKeepaliveWaitRequest::Wait);
    EXPECT_EQ(primary_karl_locator.address_port_route, generator.Claire().GetKarlLocator().address_port_route);
  }

  while (primary_karl.ActiveServicesCount() == 0u) {
    std::this_thread::yield();
  }
  // The `generator` service is again registered as active in the primary `Karl`.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(1u, per_ip_services.size());
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
  }
}

// NOTE(dkorolev): On my Ubuntu 16.04, when run as:
// $ ./.current/test --gtest_filter=Karl.ChangeDependenciesInClaire --gtest_repeat=-1
// this test consistently fails on iteration 253, when calling `::getaddrinfo()` for `localhost`.
// Changing `localhost` into `127.0.0.1` fails to create a socket (`::socket()` returns -1).
// While there's a nonzero chance we're leaking sockets here, I'm postponing the investigation for now. -- D.K.
TEST(Karl, ChangeDependenciesInClaire) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
  const uint16_t claire_port = PickPortForUnitTest();
  current::karl::Claire claire(karl_locator, "unittest", claire_port, {"http://127.0.0.1:12345"});
  // Register with no custom status filler and wait for the confirmation from Karl.
  claire.Register(nullptr, true);

  // Karl sees `claire`'s single dependency on `127.0.0.1:12345` as unresolved.
  {
    unittest_karl_status_t status;
    const auto body = HTTP(GET(Printf("http://localhost:%d?full&from=0", FLAGS_karl_test_fleet_view_port))).body;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(body)) << body;
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& server = status.machines["127.0.0.1"];
    auto& per_ip_services = server.services;
    ASSERT_EQ(1u, per_ip_services.size());
    auto& claire_service_info = per_ip_services[claire.Codename()];
    ASSERT_EQ("unittest", claire_service_info.service);
    EXPECT_EQ(0u, claire_service_info.dependencies.size());
    auto& unresolved_dependencies = claire_service_info.unresolved_dependencies;
    ASSERT_EQ(1u, unresolved_dependencies.size());
    ASSERT_EQ("http://127.0.0.1:12345/.current", unresolved_dependencies[0]);
  }

  // Launch another Claire to create a resolvable dependency.
  const int claire_to_depend_on_update_ts = 1000;
  current::time::SetNow(std::chrono::microseconds(claire_to_depend_on_update_ts),
                        std::chrono::microseconds(claire_to_depend_on_update_ts + 100));
  const uint16_t claire_to_depend_on_port = PickPortForUnitTest();
  current::karl::Claire claire_to_depend_on(karl_locator, "unittest", claire_to_depend_on_port);
  // Register with no custom status filler and wait for the confirmation from Karl.
  claire_to_depend_on.Register(nullptr, true);

  // Modify `claire`'s dependencies list and notify Karl.
  const int last_update_ts = 2000;
  current::time::SetNow(std::chrono::microseconds(last_update_ts), std::chrono::microseconds(last_update_ts + 100));
  const std::string dependency_url = Printf("http://127.0.0.1:%d", claire_to_depend_on_port);
  claire.SetDependencies({dependency_url});
  claire.ForceSendKeepalive(current::karl::ForceSendKeepaliveWaitRequest::Wait);

  // Wait for Karl to receive `claire`'s latest keepalive.
  {
    const std::string karl_status_url =
        Printf("http://localhost:%d?full&from=%d", FLAGS_karl_test_fleet_view_port, last_update_ts);
    bool karl_received_last_keepalive = false;
    while (!karl_received_last_keepalive) {
      const auto status = TryParseJSON<unittest_karl_status_t>(HTTP(GET(karl_status_url)).body);
      ASSERT_TRUE(Exists(status));
      karl_received_last_keepalive = Value(status).machines.size() > 0u;
    }
  }

  // Karl lists a resolved dependency for `claire`.
  {
    // We should pass `claire_to_depend_on_update_ts` as `from` argument for proper dependency resolution.
    const std::string karl_status_url =
        Printf("http://localhost:%d?full&from=%d", FLAGS_karl_test_fleet_view_port, claire_to_depend_on_update_ts);
    unittest_karl_status_t status;
    ASSERT_NO_THROW(status = ParseJSON<unittest_karl_status_t>(HTTP(GET(karl_status_url)).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& server = status.machines["127.0.0.1"];
    auto& per_ip_services = server.services;
    ASSERT_EQ(2u, per_ip_services.size());
    auto& claire_service_info = per_ip_services[claire.Codename()];
    ASSERT_EQ("unittest", claire_service_info.service);
    EXPECT_EQ(0u, claire_service_info.unresolved_dependencies.size());
    auto& dependencies = claire_service_info.dependencies;
    ASSERT_EQ(1u, dependencies.size());
    ASSERT_EQ(claire_to_depend_on.Codename(), dependencies[0]);
  }
}

TEST(Karl, ClaireNotifiesUserObject) {
  using namespace karl_unittest;

  current::time::ResetToZero();

  struct ClaireNotifiable : current::karl::IClaireNotifiable {
    std::vector<std::string> karl_urls;
    void OnKarlLocatorChanged(const current::karl::Locator& locator) override {
      karl_urls.push_back(locator.address_port_route);
    }
  };

  current::karl::Locator karl("http://localhost:12345/");
  ClaireNotifiable claire_notifications_receiver;
  const uint16_t claire_port = PickPortForUnitTest();
  current::karl::Claire claire(karl, "unittest", claire_port, claire_notifications_receiver);

  // Switch Claire's Karl locator via HTTP request.
  {
    const std::string karl_url = "http://host1:10000/";
    const std::string report_to_url =
        Printf("http://localhost:%d/.current?report_to=%s", claire_port, karl_url.c_str());
    const auto response = HTTP(POST(report_to_url, ""));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ(karl_url, claire.GetKarlLocator().address_port_route);
  }

  // Switch Claire's Karl locator by calling the member function.
  {
    const std::string karl_url = "http://host2:10001/";
    claire.SetKarlLocator(current::karl::Locator(karl_url), current::karl::ForceSendKeepaliveWaitRequest::Wait);
    EXPECT_EQ(karl_url, claire.GetKarlLocator().address_port_route);
  }

  EXPECT_EQ("http://host1:10000/,http://host2:10001/",
            current::strings::Join(claire_notifications_receiver.karl_urls, ','));
}

TEST(Karl, ModifiedClaireBoilerplateStatus) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
  const uint16_t claire_port = PickPortForUnitTest();
  current::karl::Claire claire(karl_locator, "unittest", claire_port);
  claire.BoilerplateStatus().cloud_instance_name = "test_instance";
  claire.BoilerplateStatus().cloud_availability_group = "us-west-1";
  // Register with no custom status filler and wait for the confirmation from Karl.
  claire.Register(nullptr, true);

  // Modified boilerplate status is propagated to Karl.
  {
    unittest_karl_status_t status;
    ASSERT_NO_THROW(
        status = ParseJSON<unittest_karl_status_t>(
            HTTP(GET(Printf("http://localhost:%d?from=0&full&active_only", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& server = status.machines["127.0.0.1"];
    auto& per_ip_services = server.services;
    EXPECT_EQ(1u, per_ip_services.size());
    EXPECT_EQ("unittest", per_ip_services[claire.Codename()].service);
    ASSERT_TRUE(Exists(server.cloud_instance_name));
    EXPECT_EQ("test_instance", Value(server.cloud_instance_name));
    ASSERT_TRUE(Exists(server.cloud_availability_group));
    EXPECT_EQ("us-west-1", Value(server.cloud_availability_group));
  }
}

// To run a `curl`-able test: ./.current/test --karl_run_test_forever --gtest_filter=Karl.EndToEndTest
TEST(Karl, EndToEndTest) {
  current::time::ResetToZero();

  if (FLAGS_karl_run_test_forever) {
    // Instructions:
    // * Generator, Annotator, Filter: Exposed as Stream streams; curl `?size`, `?i=$INDEX&n=$COUNT`.
    // * IsPrime: Exposing a single endpoint; curl `?x=42` or `?x=43` to test.
    // * Karl: Displaying status; curl ``.
    // TODO(dkorolev): Have Karl expose the DOT/SVG, over the desired period of time. And test it.
    std::cerr << "Fleet view :: localhost:" << FLAGS_karl_test_fleet_view_port << '\n';
    std::cerr << "Generator  :: localhost:" << FLAGS_karl_generator_test_port << "/numbers\n";
    std::cerr << "IsPrime    :: localhost:" << FLAGS_karl_is_prime_test_port << "/is_prime?x=42\n";
    std::cerr << "Annotator  :: localhost:" << FLAGS_karl_annotator_test_port << "/annotated\n";
    std::cerr << "Filter     :: localhost:" << FLAGS_karl_filter_test_port << "/primes\n";
#ifndef CURRENT_CI
    if (!FLAGS_karl_nginx_config_file.empty()) {
      std::cerr << "Nginx view :: localhost:" << FLAGS_karl_nginx_port << '\n';
    }
#endif  // CURRENT_CI
  }

  auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
#ifndef CURRENT_CI
  if (!FLAGS_karl_nginx_config_file.empty()) {
    params.SetNginxParameters(current::karl::KarlNginxParameters(FLAGS_karl_nginx_port, FLAGS_karl_nginx_config_file));
  }
#endif  // CURRENT_CI
  params.svg_name = "Karl's Unit Test";
  params.github_repo_url = "https://github.com/dkorolev/Current";
  const unittest_karl_t karl(params);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
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
                        HTTP(GET(Printf("http://localhost:%d?from=0&full", FLAGS_karl_test_fleet_view_port))).body));
    EXPECT_EQ(1u, status.machines.size()) << JSON(status);
    ASSERT_TRUE(status.machines.count("127.0.0.1")) << JSON(status);
    auto& per_ip_services = status.machines["127.0.0.1"].services;
    EXPECT_EQ(4u, per_ip_services.size());
    EXPECT_EQ("annotator", per_ip_services[annotator.ClaireCodename()].service);
    EXPECT_EQ("filter", per_ip_services[filter.ClaireCodename()].service);
    EXPECT_EQ("generator", per_ip_services[generator.ClaireCodename()].service);
    EXPECT_EQ("is_prime", per_ip_services[is_prime.ClaireCodename()].service);
  }

  {
    const auto ips = karl.LocalIPs();
    ASSERT_EQ(1u, ips.size());
    EXPECT_EQ("127.0.0.1", *ips.begin());
  }

  if (FLAGS_karl_run_test_forever) {
    std::cerr << "Running forever, CTRL+C to terminate.\n";
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

TEST(Karl, KarlNotifiesUserObject) {
  current::time::ResetToZero();

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);

  struct KarlNotifiable
      : current::karl::IKarlNotifiable<Variant<current::karl::default_user_status::status, karl_unittest::is_prime>> {
    std::vector<std::string> events;
    std::atomic_size_t events_count;
    KarlNotifiable() : events_count(0u) {}
    void OnKeepalive(std::chrono::microseconds,
                     const current::karl::ClaireServiceKey&,
                     const std::string& codename,
                     const current::karl::ClaireServiceStatus<Variant<current::karl::default_user_status::status,
                                                                      karl_unittest::is_prime>>& status) override {
      // First `Exists` for `Optional<>`, second for `Variant<>`.
      if (!Exists(status.runtime) || !Exists<karl_unittest::is_prime>(Value(status.runtime))) {
        events.push_back("Keepalive: " + codename);
      } else {
        events.push_back("PrimeKeepalive: " + codename);
      }
      ++events_count;
    }
    void OnDeregistered(std::chrono::microseconds,
                        const std::string& codename,
                        const ImmutableOptional<current::karl::ClaireInfo>&) override {
      events.push_back("Deregistered: " + codename);
      ++events_count;
    }
    void OnTimedOut(std::chrono::microseconds,
                    const std::string& codename,
                    const ImmutableOptional<current::karl::ClaireInfo>&) override {
      events.push_back("TimedOut: " + codename);
      ++events_count;
    }
  };

  KarlNotifiable karl_notifications_receiver;

  const unittest_karl_t karl(params, karl_notifications_receiver);

  const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));

  std::vector<std::string> expected;
  EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));

  // First, the end-to-end test with respect to callbacks.
  {
    const karl_unittest::ServiceGenerator generator(
        FLAGS_karl_generator_test_port, std::chrono::microseconds(1000000), karl_locator);
    expected.push_back("Keepalive: " + generator.ClaireCodename());
    while (karl_notifications_receiver.events_count != expected.size()) {
      std::this_thread::yield();
    }

    EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));

    const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
    expected.push_back("PrimeKeepalive: " + is_prime.ClaireCodename());
    while (karl_notifications_receiver.events_count != expected.size()) {
      std::this_thread::yield();
    }

    EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));

    {
      const karl_unittest::ServiceAnnotator annotator(FLAGS_karl_annotator_test_port,
                                                      Printf("http://localhost:%d", FLAGS_karl_generator_test_port),
                                                      Printf("http://localhost:%d", FLAGS_karl_is_prime_test_port),
                                                      karl_locator);
      expected.push_back("Keepalive: " + annotator.ClaireCodename());
      while (karl_notifications_receiver.events_count != expected.size()) {
        std::this_thread::yield();
      }

      EXPECT_EQ(current::strings::Join(expected, ", "),
                current::strings::Join(karl_notifications_receiver.events, ", "));

      {
        const karl_unittest::ServiceFilter filter(
            FLAGS_karl_filter_test_port, Printf("http://localhost:%d", FLAGS_karl_annotator_test_port), karl_locator);
        expected.push_back("Keepalive: " + filter.ClaireCodename());
        EXPECT_EQ(current::strings::Join(expected, ", "),
                  current::strings::Join(karl_notifications_receiver.events, ", "));
        expected.push_back("Deregistered: " + filter.ClaireCodename());
      }
      while (karl_notifications_receiver.events.size() != expected.size()) {
        std::this_thread::yield();
      }
      EXPECT_EQ(current::strings::Join(expected, ", "),
                current::strings::Join(karl_notifications_receiver.events, ", "));
      expected.push_back("Deregistered: " + annotator.ClaireCodename());
    }
    while (karl_notifications_receiver.events.size() != expected.size()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));
    expected.push_back("Deregistered: " + is_prime.ClaireCodename());
    expected.push_back("Deregistered: " + generator.ClaireCodename());
  }
  while (karl_notifications_receiver.events_count != expected.size()) {
    std::this_thread::yield();
  }
  EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));

  while (karl.ActiveServicesCount()) {
    std::this_thread::yield();
  }

  // Now, the timeout test with respect to callbacks.
  {
    current::karl::ClaireStatus claire;
    claire.service = "unittest";
    claire.codename = "ABCDEF";
    claire.local_port = 8888;
    ASSERT_EQ(0u, karl.ActiveServicesCount());
    {
      const std::string keepalive_url = Printf("%s?codename=%s&port=%d",
                                               karl_locator.address_port_route.c_str(),
                                               claire.codename.c_str(),
                                               claire.local_port);
      const auto response = HTTP(POST(keepalive_url, claire));
      EXPECT_EQ(200, static_cast<int>(response.code));
      while (karl.ActiveServicesCount() != 1u) {
        std::this_thread::yield();
      }
    }
    expected.push_back("Keepalive: ABCDEF");
    EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));

    current::time::SetNow(std::chrono::microseconds(100 * 1000 * 1000), std::chrono::microseconds(101 * 1000 * 1000));
    while (Value(
        karl.BorrowStorage()->ReadOnlyTransaction([&](ImmutableFields<unittest_karl_t::storage_t> fields) -> bool {
          EXPECT_TRUE(Exists(fields.claires[claire.codename]));
          return Value(fields.claires[claire.codename]).registered_state ==
                 current::karl::ClaireRegisteredState::DisconnectedByTimeout;
        }).Go())) {
      std::this_thread::yield();
    }

    expected.push_back("TimedOut: ABCDEF");
    while (karl_notifications_receiver.events.size() != expected.size()) {
      std::this_thread::yield();
    }

    EXPECT_EQ(current::strings::Join(expected, ", "), current::strings::Join(karl_notifications_receiver.events, ", "));
  }
}

namespace karl_unittest {

CURRENT_STRUCT(CustomField) {
  CURRENT_FIELD(id, uint32_t);
  CURRENT_USE_FIELD_AS_KEY(id);
  CURRENT_FIELD(s, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(CustomField) {}
  CURRENT_CONSTRUCTOR(CustomField)(uint32_t id, const std::string& s) : id(id), s(s) {}
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, CustomField, CustomFieldDictionary);

CURRENT_STORAGE(CustomKarlStorage) {
  // Required storage fields for Karl.
  CURRENT_STORAGE_FIELD(karl, current::karl::KarlInfoDictionary);
  CURRENT_STORAGE_FIELD(claires, current::karl::ClaireInfoDictionary);
  CURRENT_STORAGE_FIELD(builds, current::karl::BuildInfoDictionary);
  CURRENT_STORAGE_FIELD(servers, current::karl::ServerInfoDictionary);
  // Custom field.
  CURRENT_STORAGE_FIELD(custom_field, CustomFieldDictionary);
};

}  // namespace karl_unittest

TEST(Karl, CustomStorage) {
  using namespace karl_unittest;
  current::time::ResetToZero();

  using custom_storage_t = CustomKarlStorage<StreamStreamPersister>;
  using custom_karl_t =
      current::karl::GenericKarl<custom_storage_t, current::karl::default_user_status::status, karl_unittest::is_prime>;

  const auto params = UnittestKarlParameters();
  const auto stream_file_remover = current::FileSystem::ScopedRmFile(params.stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(params.storage_persistence_file);
  auto storage = custom_storage_t::CreateMasterStorage(params.storage_persistence_file);

  {
    const auto result = storage->ReadWriteTransaction([](MutableFields<custom_storage_t> fields) {
      fields.custom_field.Add(CustomField(42, "UnitTest"));
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string generator_codename;
  {
    const custom_karl_t karl(storage, params);
    const current::karl::Locator karl_locator(Printf("http://localhost:%d/", FLAGS_karl_test_keepalives_port));
    const karl_unittest::ServiceGenerator generator(
        FLAGS_karl_generator_test_port, std::chrono::microseconds(1000), karl_locator);
    generator_codename = generator.ClaireCodename();

    const auto result = karl.BorrowStorage()->ReadOnlyTransaction([&](ImmutableFields<custom_storage_t> fields) {
      ASSERT_TRUE(Exists(fields.claires[generator_codename]));
      EXPECT_EQ(current::karl::ClaireRegisteredState::Active,
                Value(fields.claires[generator_codename]).registered_state);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  {
    const auto result = storage->ReadOnlyTransaction([&](ImmutableFields<custom_storage_t> fields) {
      // `generator` has deregistered itself on destruction.
      ASSERT_TRUE(Exists(fields.claires[generator_codename]));
      EXPECT_EQ(current::karl::ClaireRegisteredState::Deregistered,
                Value(fields.claires[generator_codename]).registered_state);
      // Our `custom_field` is fine.
      ASSERT_TRUE(Exists(fields.custom_field[42]));
      EXPECT_EQ("UnitTest", Value(fields.custom_field[42]).s);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}

#if 0
// TODO(dkorolev): This test is incomplete; revisit it some day soon. Thanks for understanding.
TEST(Karl, Visualization) {
  current::time::ResetToZero();
  using variant_t = Variant<current::karl::default_user_status::status, karl_unittest::is_prime>;
  current::karl::GenericKarlStatus<variant_t> status;
  {
    current::karl::ServerToReport<variant_t>& server1 = status.machines["10.0.0.1"];
    server1.time_skew = "local time synchnorized";
    {
      current::karl::ServiceToReport<variant_t> master;
      master.currently = current::karl::current_service_state::up(
          std::chrono::microseconds(123), "just now", std::chrono::microseconds(456), "up 1d 20h 42m 27s");
      master.service = "service";
      master.codename = "AAAAAA";
      master.location = current::karl::ClaireServiceKey("10.0.0.1:10001");
      master.dependencies = {};
      master.url_status_page_proxied = "http://127.0.0.1:7576/AAAAAA";
      master.url_status_page_direct = "http://10.0.0.1:10001/.current";
      master.runtime = karl_unittest::is_prime(100);
      server1.services[master.codename] = master;
    }
  }
  {
    current::karl::ServerToReport<variant_t>& server2 = status.machines["10.0.0.2"];
    server2.time_skew = "0m 42s";
  }

  const auto graph = Render(status);

  const auto filename_prefix = current::FileSystem::JoinPath("golden", "viz");
  if (FLAGS_karl_overwrite_golden_files) {
    current::FileSystem::WriteStringToFile(graph.AsDOT(), (filename_prefix + ".dot").c_str());
    current::FileSystem::WriteStringToFile(graph.AsSVG(), (filename_prefix + ".svg").c_str());
  }

  EXPECT_EQ(current::FileSystem::ReadFileAsString(filename_prefix + ".dot"), graph.AsDOT());
}
#endif

#endif  // #ifndef CURRENT_CI_TRAVIS for the second half of Travis tests.
