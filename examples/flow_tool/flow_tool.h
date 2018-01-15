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

  // The filesystem traversal logic.
  struct NodeSearchResult {
    const db::Node* node = nullptr;
    Optional<Response> error;
    explicit NodeSearchResult(const db::Node* node) : node(node) { assert(node != nullptr); }
    explicit NodeSearchResult(Response error) : error(std::move(error)) {}
  };

  // Accepts both read-only and read-write transactions.
  template <typename FIELDS>
  NodeSearchResult FindNodeFromWithinTransactionImpl(FIELDS&& fields, const std::vector<std::string>& path) {
    const auto optional_root_node = fields.node[db::node_key_t::RootNode];
    if (!Exists(optional_root_node) || !Exists<db::Dir>(Value(optional_root_node).data)) {
      return NodeSearchResult(Response(api::error::Error("InternalFileSystemIntegrityError",
                                                         "The root of the internal filesystem is not a directory."),
                                       HTTPResponseCode.InternalServerError));
    } else {
      const db::Node* result = nullptr;
      if (path.empty()) {
        return NodeSearchResult(&Value(optional_root_node));
      } else {
        const db::Dir* ptr = &Value<db::Dir>(Value(optional_root_node).data);
        size_t i = 0;
        while (i < path.size() && ptr && !result) {
          const db::Dir* next_ptr = nullptr;
          for (const auto& e : ptr->dir) {
            const auto optional_next_node = fields.node[e];
            if (!Exists(optional_next_node)) {
              api::error::InternalFileSystemIntegrityError error;
              error.path = '/' + current::strings::Join(path, '/');
              error.message = "The internal filesystem contains an invalid entry.";
              error.error_component = path[i];
              error.error_component_zero_based_index = i;
              return NodeSearchResult(Response(error, HTTPResponseCode.InternalServerError));
            }
            const auto& next_node = Value(optional_next_node);
            if (next_node.name == path[i]) {
              if (i + 1u == path.size()) {
                result = &next_node;
                break;
              } else {
                if (!Exists<db::Dir>(next_node.data)) {
                  api::error::ErrorAtteptedToAccessFileAsDir error;
                  error.path = '/' + current::strings::Join(path, '/');
                  error.message = "The internal filesystem contains an invalid entry.";
                  error.file_component = path[i];
                  error.file_component_zero_based_index = i;
                  Response(error, HTTPResponseCode.BadRequest);
                } else {
                  next_ptr = &Value<db::Dir>(next_node.data);
                  break;
                }
              }
            }
          }
          if (next_ptr) {
            ptr = next_ptr;
            ++i;
          } else {
            break;
          }
        }
        if (result) {
          return NodeSearchResult(result);
        } else {
          api::error::ErrorPathNotFound error;
          error.path = '/' + current::strings::Join(path, '/');
          error.not_found_component = path[i];
          error.not_found_component_zero_based_index = i;
          return NodeSearchResult(Response(error, HTTPResponseCode.NotFound));
        }
      }
    }
  }

  NodeSearchResult FindNodeFromWithinTransaction(ImmutableFields<storage_t> fields,
                                                 const std::vector<std::string>& path) {
    return FindNodeFromWithinTransactionImpl(fields, path);
  }

  NodeSearchResult FindNodeFromWithinTransaction(MutableFields<storage_t> fields,
                                                 const std::vector<std::string>& path) {
    return FindNodeFromWithinTransactionImpl(fields, path);
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
              [this, path](ImmutableFields<storage_t> fields) -> Response {
                const NodeSearchResult result = FindNodeFromWithinTransaction(fields, path);
                if (result.node) {
                  struct ResponseGenerator {
                    const ImmutableFields<storage_t>& fields;
                    const std::vector<std::string>& path;
                    ResponseGenerator(const ImmutableFields<storage_t>& fields, const std::vector<std::string>& path)
                        : fields(fields), path(path) {}
                    Response response;
                    void FillProtoFields(api::success::FileOrDirResponse& proto_response_object) {
                      proto_response_object.url = "smoke_test_passed://" + current::strings::Join(path, '/');
                      proto_response_object.path = '/' + current::strings::Join(path, '/');
                    }
                    void operator()(const db::File& file) {
                      api::success::FileResponse response_object;
                      FillProtoFields(response_object);
                      const auto blob = fields.blob[file.blob];
                      if (Exists(blob)) {
                        response_object.data = Value(blob).body;
                        response = Response(response_object);
                      } else {
                        response = Response(api::error::Error("InternalError", "The target blob was not found."),
                                            HTTPResponseCode.InternalServerError);
                      }
                    }
                    void operator()(const db::Dir& dir) {
                      api::success::DirResponse response_object;
                      FillProtoFields(response_object);
                      for (const auto& e : dir.dir) {
                        const auto node = fields.node[e];
                        if (Exists(node)) {
                          response_object.dir.push_back(Value(node).name);
                        } else {
                          response = Response(
                              api::error::Error("InternalError", "The target node dir refers to a non-existing node."),
                              HTTPResponseCode.InternalServerError);
                          return;
                        }
                      }
                      response = Response(response_object);
                    }
                  };
                  ResponseGenerator generator(fields, path);
                  result.node->data.Call(generator);
                  return std::move(generator.response);
                } else {
                  assert(Exists(result.error));
                  return Value(result.error);
                }
              },
              std::move(r))
          .Go();
    } else if (r.method == "PUT") {
      // TODO(dkorolev): Allow creating directories too.
      // TODO(dkorolev): Overall test that no directory is overwritten with a file or vice versa.
      const auto url_path_args = r.url_path_args;
      const std::vector<std::string> path(url_path_args.begin(), url_path_args.end());
      const std::string body = r.body;  // TODO(dkorolev): This `body` should be more complex.
      if (path.empty()) {
        r(api::error::Error("FilesystemError", "Attempted to overwrite the root `/` directory with a file."),
          HTTPResponseCode.BadRequest);
        return;
      }
      storage_
          ->ReadWriteTransaction(
              [this, path, body](MutableFields<storage_t> fields) -> Response {
                const NodeSearchResult result_file = FindNodeFromWithinTransaction(fields, path);
                if (result_file.node) {
                  if (Exists<db::File>(result_file.node->data)) {
                    const auto optional_blob = fields.blob[Value<db::File>(result_file.node->data).blob];
                    if (!Exists(optional_blob)) {
                      return Response(api::error::Error("InternalError", "The target blob was not found."),
                                      HTTPResponseCode.InternalServerError);
                    } else {
                      db::Blob blob = Value(optional_blob);
                      blob.body = body;
                      fields.blob.Add(blob);
                      return "Dima: OK.\n";  // TODO(dkorolev): A better message.
                    }
                  } else {
                    return Response(
                        api::error::Error("FilesystemError", "Attempted to overwrite directory with a file."),
                        HTTPResponseCode.BadRequest);
                  }
                } else {
                  const std::string filename = path.back();
                  const std::vector<std::string> dir_path(path.begin(), --path.end());
                  const NodeSearchResult result_dir = FindNodeFromWithinTransaction(fields, dir_path);
                  if (result_dir.node) {
                    if (Exists<db::Dir>(result_dir.node->data)) {
                      const auto optional_dir_node = fields.node[result_dir.node->key];
                      if (!Exists(optional_dir_node)) {
                        return Response(api::error::Error("InternalError", "The assumed directory does not exist."),
                                        HTTPResponseCode.InternalServerError);
                      } else {
                        db::Node dir_node = Value(optional_dir_node);

                        db::Blob file_blob;
                        file_blob.key = db::Blob::GenerateRandomBlobKey();
                        file_blob.body = body;
                        fields.blob.Add(file_blob);

                        db::Node file_node;
                        file_node.key = db::Node::GenerateRandomFileKey();
                        file_node.name = filename;
                        file_node.data = db::File();
                        Value<db::File>(file_node.data).blob = file_blob.key;
                        fields.node.Add(file_node);

                        Value<db::Dir>(dir_node.data).dir.push_back(file_node.key);
                        fields.node.Add(dir_node);

                        return "Dima: OK.\n";  // TODO(dkorolev): A better message.
                      }
                    } else {
                      return Response(api::error::Error("FilesystemError", "Attempted to use a file as a directory."),
                                      HTTPResponseCode.BadRequest);
                    }
                  } else {
                    // Neither file nor dir were found.
                    assert(Exists(result_dir.error));
                    return Value(result_dir.error);
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
