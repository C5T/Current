/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef EXAMPLES_FLOW_TOOL_FLOW_TOOL_H
#define EXAMPLES_FLOW_TOOL_FLOW_TOOL_H

#include "schema.h"

#include "../../blocks/http/api.h"
#include "../../storage/persister/stream.h"

namespace flow_tool {

template <template <template <typename...> class, template <typename> class, typename> class CURRENT_STORAGE_TYPE,
          template <typename...> class DB_PERSISTER>
class FlowTool final {
 public:
  using transaction_t = current::storage::transaction_t<CURRENT_STORAGE_TYPE>;
  using stream_type_t = transaction_t;

  using storage_t =
      CURRENT_STORAGE_TYPE<DB_PERSISTER, current::storage::transaction_policy::Synchronous, stream_type_t>;
  using stream_t = typename storage_t::stream_t;

 private:
  const std::chrono::microseconds start_time_epoch_us_;
  std::chrono::microseconds ready_time_epoch_us_;
  current::Owned<stream_t> stream_;
  current::Owned<storage_t> storage_;
  HTTPRoutesScope http_routes_scope_;

 public:
  template <typename... ARGS>
  FlowTool(uint16_t port, const std::string& url_prefix, ARGS&&... args)
      : start_time_epoch_us_(current::time::Now()),
        ready_time_epoch_us_(start_time_epoch_us_),  // To indicate we're still in the `initializing` mode.
        stream_(stream_t::CreateStream(std::forward<ARGS>(args)...)),
        storage_(storage_t::CreateMasterStorageAtopExistingStream(stream_)),
        http_routes_scope_(
            HTTP(port).Register(url_prefix + "/up", [this](Request r) { ServeHealthz(std::move(r)); }) +
            HTTP(port).Register(url_prefix + "/healthz", [this](Request r) { ServeHealthz(std::move(r)); }) +
            HTTP(port).Register(
                url_prefix + "/tree", URLPathArgs::CountMask::Any, [this](Request r) { ServeTree(std::move(r)); })) {
    // Make sure to create a top-level filesystem node of the directory type when starting from scratch.
    storage_
        ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
          if (!fields.node.Has(db::node_key_t::RootNode)) {
            db::Node root_node;
            root_node.key = db::node_key_t::RootNode;
            root_node.name = "";
            root_node.data = db::Dir();
            fields.node.Add(root_node);
          }
        })
        .Go();
    ready_time_epoch_us_ = current::time::Now();
  }

  // The health check HTTP route.
  void ServeHealthz(Request r) {
    const auto now = current::time::Now();
    api::Healthz healthz_response;
    healthz_response.up = (ready_time_epoch_us_ != start_time_epoch_us_);
    healthz_response.uptime = current::strings::TimeIntervalAsHumanReadableString(now - start_time_epoch_us_);
    healthz_response.start_time_epoch_us = start_time_epoch_us_;
    healthz_response.init_duration_us = ready_time_epoch_us_ - start_time_epoch_us_;
    healthz_response.uptime_us = now - start_time_epoch_us_;
    r(healthz_response);
  }

  // The filesystem-handling HTTP response handler.
  void ServeTree(Request r) {
    if (r.method == "GET") {
      const auto url_path_args = r.url_path_args;
      const std::vector<std::string> path(url_path_args.begin(), url_path_args.end());
      storage_
          ->ReadOnlyTransaction(
              [path](ImmutableFields<storage_t> fields) -> Response {
                const auto optional_root_node = fields.node[db::node_key_t::RootNode];
                if (!Exists(optional_root_node) || !Exists<db::Dir>(Value(optional_root_node).data)) {
                  return Response(
                      api::Error("InternalError", "The root of the internal filesystem is not a directory."),
                      HTTPResponseCode.InternalServerError);
                } else {
                  const db::Node* result = nullptr;
                  if (path.empty()) {
                    result = &Value(optional_root_node);
                  } else {
                    const db::Dir* ptr = &Value<db::Dir>(Value(optional_root_node).data);
                    size_t i = 0;
                    while (i < path.size() && ptr && !result) {
                      const db::Dir* next_ptr = nullptr;
                      for (const auto& e : ptr->dir) {
                        const auto optional_next_node = fields.node[e];
                        if (!Exists(optional_next_node)) {
                          return Response(api::Error("InternalError",
                                                     "The internal filesystem contains an invalid entry at index " +
                                                         current::ToString(i) + " of path " + JSON(path) + "."),
                                          HTTPResponseCode.InternalServerError);
                        }
                        const auto& next_node = Value(optional_next_node);
                        if (next_node.name == path[i]) {
                          if (i + 1u == path.size()) {
                            result = &next_node;
                            break;
                          } else {
                            if (!Exists<db::Dir>(next_node.data)) {
                              return Response(
                                  api::Error("BadRequest",
                                             "Attempted to access a file as if it is a directory, at index " +
                                                 current::ToString(i) + " of path " + JSON(path) + "."),
                                  HTTPResponseCode.BadRequest);
                            } else {
                              next_ptr = &Value<db::Dir>(next_node.data);
                              break;
                            }
                          }
                        }
                      }
                      ptr = next_ptr;
                      ++i;
                    }
                  }
                  if (result) {
                    struct ResponseGenerator {
                      const ImmutableFields<storage_t>& fields;
                      const std::vector<std::string>& path;
                      ResponseGenerator(const ImmutableFields<storage_t>& fields, const std::vector<std::string>& path)
                          : fields(fields), path(path) {}
                      Response response;
                      void FillProtoFields(api::FileOrDirResponse& proto_response_object) {
                        proto_response_object.url = "smoke_test_passed://" + current::strings::Join(path, '/');
                        proto_response_object.path = path;
                      }
                      void operator()(const db::File& file) {
                        api::FileResponse response_object;
                        FillProtoFields(response_object);
                        const auto blob = fields.blob[file.blob];
                        if (Exists(blob)) {
                          response_object.data = Value(blob).body;
                          response = Response(response_object);
                        } else {
                          response = Response(api::Error("InternalError", "The target blob was not found."),
                                              HTTPResponseCode.InternalServerError);
                        }
                      }
                      void operator()(const db::Dir& dir) {
                        api::DirResponse response_object;
                        FillProtoFields(response_object);
                        for (const auto& e : dir.dir) {
                          const auto node = fields.node[e];
                          if (Exists(node)) {
                            response_object.dir.push_back(Value(node).name);
                          } else {
                            response = Response(
                                api::Error("InternalError", "The target node dir refers to a non-existing node."),
                                HTTPResponseCode.InternalServerError);
                            return;
                          }
                        }
                        response = Response(response_object);
                      }
                    };
                    ResponseGenerator generator(fields, path);
                    result->data.Call(generator);
                    return std::move(generator.response);
                  } else {
                    return Response(
                        api::Error("NotFound", "The specified directory does not contain the requested file."),
                        HTTPResponseCode.NotFound);
                  }
                }
              },
              std::move(r))
          .Go();
    } else {
      r("Other methods coming soon.\n", HTTPResponseCode.MethodNotAllowed);
    }
  }

  // For the unit tests.
  current::WeakBorrowed<storage_t>& MutableDB() { return storage_; }
  const current::WeakBorrowed<storage_t>& ImmutableDB() const { return storage_; }
};

}  // namespace flow_tool

#endif  // EXAMPLES_FLOW_TOOL_FLOW_TOOL_H
