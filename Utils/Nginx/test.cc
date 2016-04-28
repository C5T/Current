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

#include "config.h"

#include "../../TypeSystem/Serialization/json.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_bool(nginx_overwrite_golden_files, false, "Set to true to have Nginx golden files created/overwritten.");

TEST(Nginx, FullConfig) {
  using current::FileSystem;
  using namespace current::utils::nginx::config;

  FullConfig config;
  config.AddSimpleDirective("error_log", "/var/log/nginx/error.log");
  config.AddBlockDirective("events", "").AddSimpleDirective("worker_connections", "1024");
  config.http.AddSimpleDirective("log_format", "main '$remote_addr [$time_local] \"$request\"'");
  config.http.AddSimpleDirective("access_log", "/var/log/nginx/access.log main");
  auto& server = config.http.AddServer(80);
  server.AddSimpleDirective("server_name", "myserver");
  server.AddDefaultLocation("/").AddSimpleDirective("proxy_pass", "http://127.0.0.1:8888/");
  auto& complex_location = server.AddLocation("~ /a|/b");
  complex_location.AddNestedLocation("~ /a");
  complex_location.AddNestedLocation("~ /b");
  const std::string nginx_config = ConfigPrinter::AsString(config);

  const std::string golden_filename = FileSystem::JoinPath("golden", "full.conf");
  if (FLAGS_nginx_overwrite_golden_files) {
    FileSystem::WriteStringToFile(nginx_config, golden_filename.c_str());
  }

  EXPECT_EQ(FileSystem::ReadFileAsString(golden_filename), nginx_config);
}
