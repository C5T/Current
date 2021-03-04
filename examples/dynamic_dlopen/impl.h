#ifndef EXAMPLES_DYNAMIC_DLOPEN_IMPL_H
#define EXAMPLES_DYNAMIC_DLOPEN_IMPL_H

#include "../../blocks/http/api.h"
#include "../../bricks/file/file.h"
#include "../../bricks/system/syscalls.h"
#include "../../bricks/util/sha256.h"
#include "../../typesystem/serialization/json.h"
#include "../../typesystem/struct.h"

#include "../iris/data/dataset.h"

#ifndef CURRENT_WINDOWS
class DynamicDLOpenIrisExampleImpl {
 private:
  using F = void (*)(const Schema&, std::ostringstream&);

  // clang-format off
  const std::string prefix_ = current::Base64Decode("I2luY2x1ZGUgPHZlY3Rvcj4KI2luY2x1ZGUgPHNzdHJlYW0+CiNpbmNsdWRlIDxpb21hbmlwPgoKc3RydWN0IEJhc2UgewogIHZpcnR1YWwgfkJhc2UoKSB7fQp9OwoKc3RydWN0IEZsb3dlciA6IEJhc2UgewogIHZpcnR1YWwgfkZsb3dlcigpIHt9CiAgdW5pb24gewogICAgc3RydWN0IHsKICAgICAgZG91YmxlIHNsOwogICAgICBkb3VibGUgc3c7CiAgICAgIGRvdWJsZSBwbDsKICAgICAgZG91YmxlIHB3OwogICAgfTsKICAgIGRvdWJsZSB4WzRdOwogIH07CiAgc3RkOjpzdHJpbmcgbGFiZWw7Cn07CgpleHRlcm4gIkMiIHZvaWQgUnVuKGNvbnN0IHN0ZDo6dmVjdG9yPEZsb3dlcj4mIGZsb3dlcnMsIHN0ZDo6b3N0cmluZ3N0cmVhbSYgb3MpIHsK");
  const std::string suffix_ = "\n}\n";
  // clang-format on

  Schema data_;  // `Schema` is already the vector containing all Irises.
  HTTPRoutesScope routes_;

  struct CompiledUserFunction {
    current::bricks::system::JITCompiledCPP jit;
    F f;
    explicit CompiledUserFunction(std::string code) : jit(code), f(jit.template Get<F>("Run")) {}
  };
  std::unordered_map<std::string, std::unique_ptr<CompiledUserFunction>> handlers_;

  HTTPRoutesScope RegisterRoutes(uint16_t port) {
    return HTTP(port).Register("/", URLPathArgs::CountMask::Any, [this](Request r) { Serve(std::move(r)); });
  }

  void Serve(Request r) {
    if (r.method == "POST") {
      std::string const body = r.body;
      std::string const sha = current::SHA256(body);
      if (handlers_.count(sha)) {
        r(Response("Already available: @" + sha + '\n').SetHeader("X-SHA", sha).Code(HTTPResponseCode.Created));
      } else {
        try {
          const std::chrono::microseconds t_begin = current::time::Now();
          handlers_.emplace(sha, std::make_unique<CompiledUserFunction>(prefix_ + body + suffix_));
          const std::chrono::microseconds t_end = current::time::Now();
          const double ms = 0.001 * (t_end - t_begin).count();
          r(Response("Compiled: @" + sha + '\n')
                .SetHeader("X-SHA", sha)
                .SetHeader("X-Compile-MS", current::strings::Printf("%.1lf", ms))
                .Code(HTTPResponseCode.Created));
        } catch (const current::Exception& e) {
          r(Response("*** ERROR ***\n\n" + e.OriginalDescription() + '\n').Code(HTTPResponseCode.BadRequest));
        }
      }
    } else if (r.url_path_args.size() >= 1) {
      if (r.url_path_args[0] == "boilerplate") {
        r(prefix_ + "### USER CODE ###" + suffix_);
      } else {
        const auto cit = handlers_.find(r.url_path_args[0]);
        if (cit != handlers_.end()) {
          std::ostringstream os;
          const std::chrono::microseconds t_begin = current::time::Now();
          cit->second->f(data_, os);
          const std::chrono::microseconds t_end = current::time::Now();
          const double ms = 0.001 * (t_end - t_begin).count();
          const double qps = 1000.0 / ms;
          r(Response(os.str())
                .SetHeader("X-MS", current::strings::Printf("%.1lf", ms))
                .SetHeader("X-QPS", current::strings::Printf("%.1lf", qps)));
        } else {
          r(Response("No compiled code with this SHA256 was found.\n").Code(HTTPResponseCode.NotFound));
        }
      }
    } else {
      r(Response("POST a piece of code onto `/` or GET `/${SHA256}` to execute it.\n")
            .Code(HTTPResponseCode.MethodNotAllowed)
            .SetHeader("X-Total", current::ToString(handlers_.size())));
    }
  }

 public:
  DynamicDLOpenIrisExampleImpl(Schema data, uint16_t port) : data_(std::move(data)), routes_(RegisterRoutes(port)) {}
  DynamicDLOpenIrisExampleImpl(std::string filename, uint16_t port)
      : data_(ParseJSON<Schema>(current::FileSystem::ReadFileAsString(filename))), routes_(RegisterRoutes(port)) {}

  size_t TotalFlowers() const { return data_.size(); }
};
#endif  // CURRENT_WINDOWS

#endif  // EXAMPLES_DYNAMIC_DLOPEN_IMPL_H
