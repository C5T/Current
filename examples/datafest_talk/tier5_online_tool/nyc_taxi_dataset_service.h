#ifndef EXAMPLES_DATAFEST_TALK_2008_NYC_TAXI_DATASET_SERVICE_H
#define EXAMPLES_DATAFEST_TALK_2008_NYC_TAXI_DATASET_SERVICE_H

#include "../../../blocks/http/api.h"
#include "../../../bricks/file/file.h"
#include "../../../bricks/system/syscalls.h"
#include "../../../bricks/util/sha256.h"
#include "../../../typesystem/serialization/json.h"
#include "../../../typesystem/struct.h"

#include "../tier4_cook_binary_integers/schema_integers.h"

#include "inl_files.h"

struct IterableData {
  const IntegerRide* data_buffer_;
  const size_t total_rides_;
  IterableData(const IntegerRide* data_buffer, size_t total_rides)
      : data_buffer_(data_buffer), total_rides_(total_rides) {}
  const IntegerRide* begin() const { return data_buffer_; }
  const IntegerRide* end() const { return data_buffer_ + total_rides_; }
  size_t size() const { return total_rides_; }
};

struct RunBasic {
  using external_f_t = void (*)(const IterableData&, std::ostringstream&);

  static const std::string& Boilerplate() { return static_file_boilerplate_basic_inl; }

  static std::string DoRunIntoString(const IterableData& data, external_f_t f) {
    std::ostringstream os;
    f(data, os);
    return os.str();
  }

  static void DoRun(Request r, const IterableData& data, external_f_t f) {
    std::ostringstream os;
    const std::chrono::microseconds t_begin = current::time::Now();
    f(data, os);
    const std::chrono::microseconds t_end = current::time::Now();
    const double ms = 0.001 * (t_end - t_begin).count();
    const double qps = 1000.0 / ms;
    r(Response(os.str())
          .SetHeader("X-MS", current::strings::Printf("%.1lf", ms))
          .SetHeader("X-QPS", current::strings::Printf("%.1lf", qps)));
  }

  static std::string WrapID(const std::string& id) { return "/" + id; }
};

struct RunFull {
  using external_f_t = void (*)(const IterableData&, Request);

  static const std::string& Boilerplate() { return static_file_boilerplate_full_inl; }

  static std::string DoRunIntoString(const IterableData&, external_f_t) {
    return "The results of `ENDPOINT()` calls are not necessarily textual, click the 'Link' above.";
  }

  static void DoRun(Request r, const IterableData& data, external_f_t f) { f(data, std::move(r)); }

  static std::string WrapID(const std::string& id) { return "/full/" + id; }
};

CURRENT_STRUCT(CodeIdResultError) {
  CURRENT_FIELD(code, std::string);
  CURRENT_FIELD(id, Optional<std::string>);
  CURRENT_FIELD(result, Optional<std::string>);
  CURRENT_FIELD(error, Optional<std::string>);
};

class HTMLFormGenerator {
 private:
  std::string prefix_;
  std::string suffix_;

 public:
  HTMLFormGenerator() {
    const std::string placeholder = "${CODE_RESULT_ERROR}";
    const char* location = ::strstr(static_file_interactive_html_inl.c_str(), placeholder.c_str());
    if (!location) {
      std::cerr << "Could not find '" << placeholder << "' in `interactive.html.inl`." << std::endl;
      std::exit(-1);
    }
    prefix_ = static_file_interactive_html_inl.substr(0u, (location - static_file_interactive_html_inl.c_str()));
    suffix_ = static_file_interactive_html_inl.substr((location - static_file_interactive_html_inl.c_str()) +
                                                      placeholder.length());
  }

  std::string Build(const CodeIdResultError& param) const { return prefix_ + JSON(param) + suffix_; }
};

template <class CALLER>
class NYCTaxiDatasetServiceImpl {
 private:
  using caller_t = CALLER;
  using external_f_t = typename caller_t::external_f_t;

  const IterableData& iterable_data_;
  const std::string current_dir_;

  HTTPRoutesScope routes_;

  struct CompiledUserFunction {
    current::bricks::system::JITCompiledCPP jit;
    external_f_t f;
    CompiledUserFunction(std::string code, const std::string& current_dir)
        : jit(code, current_dir), f(jit.template Get<external_f_t>("Run")) {}
  };
  std::unordered_map<std::string, std::unique_ptr<CompiledUserFunction>> handlers_;

  HTTPRoutesScope RegisterRoutes(std::string route, uint16_t port) {
    return HTTP(current::net::BarePort(port)).Register(route, URLPathArgs::CountMask::Any, [this](Request r) {
      Serve(std::move(r));
    });
  }

  std::string ConstructSource(const std::string& body, const std::string& source_name = "code.cc") const {
    std::ostringstream constructed_source;
    constructed_source << caller_t::Boilerplate();
    constructed_source << "# line 1 \"" << source_name << "\"\n";
    constructed_source << body;
    return constructed_source.str();
  }

  void HandleUpload(Request r, std::string id, std::string body) {
    if (r.url.query.has("showsource")) {
      r(ConstructSource(body, r.headers.GetOrDefault("X-Source-Name", "code.cc")));
    } else if (handlers_.count(id) && r.headers.GetOrDefault("X-Overwrite", "") != "true") {
      r(Response("Already available: @" + id + '\n').SetHeader("X-SHA", id).Code(HTTPResponseCode.Created));
    } else {
      try {
        const std::chrono::microseconds t_begin = current::time::Now();
        handlers_[id] = std::make_unique<CompiledUserFunction>(
            ConstructSource(body, r.headers.GetOrDefault("X-Source-Name", "code.cc")), current_dir_);
        const std::chrono::microseconds t_end = current::time::Now();
        const double ms = 0.001 * (t_end - t_begin).count();
        r(Response("Compiled: @" + id + '\n')
              .SetHeader("X-SHA", id)
              .SetHeader("X-Compile-MS", current::strings::Printf("%.1lf", ms))
              .Code(HTTPResponseCode.Created));
      } catch (const current::Exception& e) {
        r(Response(e.OriginalDescription()).Code(HTTPResponseCode.BadRequest));
      }
    }
  }

  void Serve(Request r) {
    const bool html = [&r]() {
      const char* kAcceptHeader = "Accept";
      if (r.headers.Has(kAcceptHeader)) {
        for (const auto& h : current::strings::Split(r.headers[kAcceptHeader].value, ',')) {
          if (current::strings::Split(h, ';').front() == "text/html") {  // Allow "text/html; charset=...", etc.
            return true;
          }
        }
      }
      return false;
    }();

    if (r.method == "POST") {
      std::string body = r.body;
      std::string id = current::SHA256(body).substr(0, 10);
      HandleUpload(std::move(r), std::move(id), std::move(body));
    } else if (r.method == "PUT") {
      if (r.url_path_args.size() == 1) {
        std::string body = r.body;
        HandleUpload(std::move(r), r.url_path_args[0], std::move(body));
      } else {
        r(Response("PUT requires a and and only URL parameter, PUT `/:id`").Code(HTTPResponseCode.BadRequest));
      }
    } else if (r.url_path_args.size() >= 1) {
      const auto cit = handlers_.find(r.url_path_args[0]);
      if (cit != handlers_.end()) {
        caller_t::DoRun(std::move(r), iterable_data_, cit->second->f);
      } else {
        r(Response("No compiled code with this ID was found.\n").Code(HTTPResponseCode.NotFound));
      }
    } else {
      if (!html) {
        r(Response(
              "POST a piece of code onto `/`, PUT a piece of code onto `/:id`, or GET `/:id[/:args]` to execute it.\n")
              .Code(HTTPResponseCode.MethodNotAllowed)
              .SetHeader("X-Total", current::ToString(handlers_.size())));
      } else {
        CodeIdResultError p;
        if (r.url.query.has("code")) {
          p.code = r.url.query["code"];
        }
        external_f_t f = nullptr;
        if (!p.code.empty()) {
          const std::string id = current::SHA256(p.code);
          p.id = caller_t::WrapID(id);
          const auto cit = handlers_.find(id);
          if (cit != handlers_.end()) {
            f = cit->second->f;
          } else {
            try {
              handlers_[id] = std::make_unique<CompiledUserFunction>(ConstructSource(p.code), current_dir_);
              f = handlers_[id]->f;
            } catch (const current::Exception& e) {
              p.error = e.OriginalDescription();
            }
          }
        }
        if (f) {
          p.result = caller_t::DoRunIntoString(iterable_data_, f);
        }
        r(Response(current::Singleton<HTMLFormGenerator>().Build(p)).ContentType("text/html; charset=utf8"));
      }
    }
  }

 public:
  NYCTaxiDatasetServiceImpl(const IterableData& iterable_data,
                            std::string route,
                            uint16_t port,
                            const std::string& current_dir)
      : iterable_data_(iterable_data), current_dir_(current_dir), routes_(RegisterRoutes(route, port)) {}
};

class NYCTaxiDatasetService {
 private:
  NYCTaxiDatasetServiceImpl<RunBasic> impl_simple_;
  NYCTaxiDatasetServiceImpl<RunFull> impl_full_;

 public:
  NYCTaxiDatasetService(const IterableData& data, uint16_t port, const std::string& current_dir)
      : impl_simple_(data, "/", port, current_dir), impl_full_(data, "/full", port, current_dir) {}
};

#endif  // EXAMPLES_DATAFEST_TALK_2008_NYC_TAXI_DATASET_SERVICE_H
