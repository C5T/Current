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

#ifndef CURRENT_UTILS_NGINX_NGINX_H
#define CURRENT_UTILS_NGINX_NGINX_H

#include <cstdlib>
#include <mutex>

#include "config.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"
#include "../../Bricks/util/random.h"
#include "../../Bricks/util/sha256.h"
#include "../../Bricks/util/singleton.h"

#include "../../Blocks/HTTP/api.h"

#ifdef CURRENT_USER_NGINX_FLAG
DECLARE_string(nginx);
#endif

namespace current {
namespace nginx {
namespace impl {

const std::string kDefaultNginxCommand = "nginx";

class NginxInvokerImpl final {
 public:
  NginxInvokerImpl()
      :
#ifdef CURRENT_USER_NGINX_FLAG
        nginx_(FLAGS_nginx)
#else
        nginx_(kDefaultNginxCommand)
#endif
  {
  }

  bool IsNginxAvailable() const { return ExecNginx("-v") == 0; }

  bool IsFullConfigValid(const std::string& config_full_path) const {
    return ExecNginx("-t -c " + config_full_path) == 0;
  }

  bool ReloadConfig() const { return ExecNginx("-s reload") == 0; }

 private:
  int ExecNginx(const std::string& args) const {
    const std::string cmd_line = nginx_ + ' ' + args + " >" + FileSystem::NullDeviceName() + " 2>&1";
    std::lock_guard<std::mutex> lock(mutex_);
    int result = std::system(cmd_line.c_str());
    return WEXITSTATUS(result);
  }

 private:
  const std::string nginx_;
  mutable std::mutex mutex_;
};

}  // namespace current::nginx::impl

inline impl::NginxInvokerImpl& NginxInvoker() { return Singleton<impl::NginxInvokerImpl>(); }

class NginxManager {
 public: 
  explicit NginxManager(const std::string& config_file) : config_file_(config_file) {
    try {
      FileSystem::WriteStringToFile("", config_file_.c_str());
    } catch (current::FileException&) {
      CURRENT_THROW(CannotWriteConfigFileException(config_file_));
    }
  }

  template <typename SERVER_DIRECTIVE>
  void UpdateConfig(SERVER_DIRECTIVE&& server_directive) {
    std::unique_lock<std::mutex> lock(mutex_);
    SERVER_DIRECTIVE directive(std::forward<SERVER_DIRECTIVE>(server_directive));
    const std::string endpoint_number = SHA256("Random location " + ToString(random::CSRandomUInt64(0u, std::numeric_limits<uint64_t>::max())));
    const std::string response_number = SHA256("Random location " + ToString(random::CSRandomUInt64(0u, std::numeric_limits<uint64_t>::max())));
    magic_numbers_ = std::make_pair(endpoint_number, response_number);
    directive.CreateLocation("/" + endpoint_number).Add(config::SimpleDirective("return", "200 \"" + response_number + "\""));
    try {
      FileSystem::WriteStringToFile(config::ConfigPrinter::AsString(directive), config_file_.c_str());
    } catch (current::FileException&) {
      CURRENT_THROW(CannotWriteConfigFileException(config_file_));
    }
    if (!NginxInvoker().ReloadConfig()) {
      CURRENT_THROW(NginxReloadConfigFailedException());
    }
    const std::string magic_url = Printf("http://localhost:%d/%s", directive.port, endpoint_number.c_str());
    bool updated = false;
#ifndef CURRENT_MOCK_TIME      
    size_t update_retries = 100u;
#else
    size_t update_retries = 10u;
#endif 
    auto WaitForNextRetry = [&update_retries]() {
      --update_retries;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    };
    while (!updated && update_retries > 0u) {
      try {
        std::cerr << "Trying to get response #" << update_retries << std::endl;
        const auto response = HTTP(GET(magic_url));
        if (response.code == HTTPResponseCode.OK && response.body == response_number) {
          std::cerr << "Success!\n";
          updated = true;
        } else {
          WaitForNextRetry();
        }
      } catch (net::NetworkException&) {
        WaitForNextRetry();
      }
    }
    if (!updated) {
      std::cerr << "Nginx hasn't properly reloaded config file '" << config_file_ << "'\nAborting." << std::endl;
      std::exit(-1);
    }
  }

  virtual ~NginxManager() {
    try {
      FileSystem::WriteStringToFile("# This file is managed by Current and has been cleaned up", config_file_.c_str());
    } catch (std::exception&) {
    }
  }

 private:
  const std::string config_file_;
  std::pair<std::string, std::string> magic_numbers_;
  std::mutex mutex_;
};

}  // namespace current::nginx
}  // namespace current

#endif  // CURRENT_UTILS_NGINX_NGINX_H
