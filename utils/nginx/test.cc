/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "nginx.h"

#include "../../typesystem/serialization/json.h"

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_bool(nginx_overwrite_golden_files, false, "Set to true to have Nginx golden files created/overwritten.");

TEST(Nginx, FullConfig) {
  using current::FileSystem;
  using namespace current::nginx::config;

  FullConfig config;
  config.Add(SimpleDirective("worker_processes", "1"));
  config.Add(BlockDirective("events", {SimpleDirective("worker_connections", "1024")}));
  config.http.Add(SimpleDirective("default_type", "application/octet-stream"))
      .Add(SimpleDirective("keepalive_timeout", "100"));
  auto& server = config.http.CreateServer(8910);
  EXPECT_THROW(config.http.CreateServer(8910), current::nginx::PortAlreadyUsedException);
  server.Add(SimpleDirective("server_name", "myserver"));
  server.CreateProxyPassLocation("/", "http://127.0.0.1:8888/");
  auto& complex_location = server.CreateLocation("~ /a|/b");
  complex_location.Add(LocationDirective("~ /a")).Add(LocationDirective("~ /b"));
  const std::string nginx_config = ConfigPrinter::AsString(config);

  const std::string golden_filename = FileSystem::JoinPath("golden", "full.conf");
  if (FLAGS_nginx_overwrite_golden_files) {
    FileSystem::WriteStringToFile(nginx_config, golden_filename.c_str());
  }

  EXPECT_EQ(FileSystem::ReadFileAsString(golden_filename), nginx_config);
}

// Disable `NginxInvoker` tests if the full test is built.
#ifndef CURRENT_MOCK_TIME
#ifndef CURRENT_CI

TEST(Nginx, NginxInvokerCheckGoldenConfig) {
  using current::FileSystem;
  using current::nginx::NginxInvoker;

  auto& nginx = NginxInvoker();
  // Run test only if Nginx is available.
  if (nginx.IsNginxAvailable()) {
    const std::string config_relative_path = FileSystem::JoinPath(FileSystem::JoinPath("..", "golden"), "full.conf");
    const std::string full_config_path = FileSystem::JoinPath(CurrentBinaryFullPath(), config_relative_path);
    EXPECT_TRUE(nginx.IsFullConfigValid(full_config_path));
  } else {
    std::cout << "Skipping the test as the Nginx binary '" << current::nginx::impl::kDefaultNginxCommand
              << "' can't be invoked.\n";
  }
}

#endif  // CURRENT_CI
#endif  // CURRENT_MOCK_TIME
