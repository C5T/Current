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
#include "../../Bricks/util/singleton.h"

#ifdef CURRENT_USER_NGINX_FLAG
DECLARE_string(nginx);
#endif

namespace current {
namespace nginx {
namespace impl {

const std::string kDefaultNginxCommand = "nginx";

class NginxInvokerImpl {
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

}  // namespace current::nginx
}  // namespace current

#endif  // CURRENT_UTILS_NGINX_NGINX_H
