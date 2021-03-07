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

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/file/file.h"
#include "../../bricks/system/syscalls.h"
#include "../../bricks/util/random.h"
#include "../../bricks/util/sha256.h"
#include "../../bricks/util/singleton.h"

#include "../../blocks/http/api.h"

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
    int result = bricks::system::SystemCall(cmd_line);
#ifndef CURRENT_WINDOWS
    return WEXITSTATUS(result);
#else
    return result;
#endif
  }

 private:
  const std::string nginx_;
  mutable std::mutex mutex_;
};

}  // namespace current::nginx::impl

inline impl::NginxInvokerImpl& NginxInvoker() { return Singleton<impl::NginxInvokerImpl>(); }

CURRENT_STRUCT(NginxManagerMagicNumbers) {
  CURRENT_FIELD(endpoint_number, std::string);
  CURRENT_FIELD(response_number, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(NginxManagerMagicNumbers)
      : endpoint_number(SHA256("Random location " +
                               ToString(random::CSRandomUInt64(0u, std::numeric_limits<uint64_t>::max())))),
        response_number(SHA256("Random response " +
                               ToString(random::CSRandomUInt64(0u, std::numeric_limits<uint64_t>::max())))) {}
};

class NginxManager {
 public:
  explicit NginxManager(const std::string& config_file) : config_file_(config_file) {
    try {
      FileSystem::WriteStringToFile("# The file is writable\n", config_file_.c_str(), true);
    } catch (current::FileException&) {
      CURRENT_THROW(CannotWriteConfigFileException(config_file_));
    }
  }

  virtual ~NginxManager() {}

  template <typename SERVER_DIRECTIVE>
  void UpdateConfig(SERVER_DIRECTIVE&& server_directive) {
    std::unique_lock<std::mutex> lock(mutex_);
    SERVER_DIRECTIVE directive(std::forward<SERVER_DIRECTIVE>(server_directive));
    magic_numbers_ = NginxManagerMagicNumbers();
    directive.CreateLocation("/" + magic_numbers_.endpoint_number)
        .Add(config::SimpleDirective("return", "200 \"" + magic_numbers_.response_number + "\""));
    try {
      FileSystem::WriteStringToFile(config::ConfigPrinter::AsString(directive), config_file_.c_str());
    } catch (current::FileException&) {
      CURRENT_THROW(CannotWriteConfigFileException(config_file_));
    }
    if (!NginxInvoker().ReloadConfig()) {
      CURRENT_THROW(NginxReloadConfigFailedException());
    }
    const std::string magic_url =
        Printf("http://localhost:%d/%s", directive.port, magic_numbers_.endpoint_number.c_str());
    bool updated = false;
#ifndef CURRENT_MOCK_TIME
    size_t update_retries = 100u;
#else
    size_t update_retries = 10u;
#endif
    const auto WaitForNextRetry = [&update_retries]() {
      if (update_retries > 0u) {
        --update_retries;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    };
    while (!updated && update_retries > 0u) {
      try {
        const auto response = HTTP(GET(magic_url));
        if (response.code == HTTPResponseCode.OK && response.body == magic_numbers_.response_number) {
          updated = true;
        } else {
          WaitForNextRetry();
        }
      } catch (net::NetworkException&) {
        WaitForNextRetry();
      }
    }
    if (!updated) {
      std::cerr << "Nginx hasn't properly reloaded config file '" << config_file_ << "'\nAborting."
                << std::endl;
      std::exit(-1);
    }
  }

  NginxManagerMagicNumbers GetMagicNumbers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return magic_numbers_;
  }

 private:
  const std::string config_file_;
  NginxManagerMagicNumbers magic_numbers_;
  mutable std::mutex mutex_;
};

}  // namespace current::nginx
}  // namespace current

#endif  // CURRENT_UTILS_NGINX_NGINX_H
