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
  const uint16_t port_;
  const std::chrono::microseconds start_time_epoch_us_;
  std::chrono::microseconds ready_time_epoch_us_;
  current::Owned<stream_t> stream_;
  current::Owned<storage_t> storage_;
  HTTPRoutesScope http_routes_scope_;

 public:
  template <typename... ARGS>
  FlowTool(uint16_t port, const std::string& url_prefix, ARGS&&... args)
      : port_(port),
        start_time_epoch_us_(current::time::Now()),
        ready_time_epoch_us_(start_time_epoch_us_),  // To indicate we're still in the `initializing` mode.
        stream_(stream_t::CreateStream(std::forward<ARGS>(args)...)),
        storage_(storage_t::CreateMasterStorageAtopExistingStream(stream_)),
        http_routes_scope_(
            HTTP(port).Register(url_prefix + "/up", [this](Request r) { ServeHealthz(std::move(r)); }) +
            HTTP(port).Register(url_prefix + "/healthz", [this](Request r) { ServeHealthz(std::move(r)); }) +
            HTTP(port).Register(
                url_prefix + "/tree", URLPathArgs::CountMask::Any, [this](Request r) { ServeTree(std::move(r)); })) {
    // Make sure to create the top-level filesystem node of the directory type if or when starting from scratch.
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

  void Join() { HTTP(port_).Join(); }

  // The filesystem traversal logic.
  struct NodeSearchResult {
    const db::Node* node = nullptr;
    Optional<Response> error;
    explicit NodeSearchResult(const db::Node* node) : node(node) { CURRENT_ASSERT(node != nullptr); }
    explicit NodeSearchResult(Response error) : error(std::move(error)) {}
  };

  // Accepts both read-only and read-write transactions.
  template <typename FIELDS>
  static NodeSearchResult FindNodeFromWithinTransactionImpl(FIELDS&& fields, const std::vector<std::string>& path) {
    const auto optional_root_node = fields.node[db::node_key_t::RootNode];
    if (!Exists(optional_root_node) || !Exists<db::Dir>(Value(optional_root_node).data)) {
      return NodeSearchResult(Response(api::error::Error("InternalFileSystemIntegrityError",
                                                         "The root of the internal filesystem is not a directory."),
                                       HTTPResponseCode.InternalServerError));
    } else {
      if (path.empty()) {
        return NodeSearchResult(&Value(optional_root_node));
      } else {
        const db::Dir* ptr = &Value<db::Dir>(Value(optional_root_node).data);
        size_t i = 0;
        while (ptr && i < path.size()) {
          const db::Dir* next_ptr = nullptr;
          for (const auto& e : ptr->dir) {
            const auto optional_next_node = fields.node[e];
            if (!Exists(optional_next_node)) {
              api::error::InternalFileSystemIntegrityError error;
              error.path = '/' + current::strings::Join(path, '/');
              error.message = "The internal filesystem contains an invalid entry.";
              error.error_component = path[i];
              error.error_component_zero_based_index = static_cast<uint32_t>(i);
              return NodeSearchResult(Response(error, HTTPResponseCode.InternalServerError));
            }
            const auto& next_node = Value(optional_next_node);
            if (next_node.name == path[i]) {
              if (i + 1u == path.size()) {
                // Success, the path was found.
                return NodeSearchResult(&next_node);
              } else {
                if (!Exists<db::Dir>(next_node.data)) {
                  api::error::ErrorAtteptedToAccessFileAsDir error;
                  error.path = '/' + current::strings::Join(path, '/');
                  error.message = "The internal filesystem contains an invalid entry.";
                  error.file_component = path[i];
                  error.file_component_zero_based_index = static_cast<uint32_t>(i);
                  return NodeSearchResult(Response(error, HTTPResponseCode.BadRequest));
                } else {
                  next_ptr = &Value<db::Dir>(next_node.data);
                  break;
                }
              }
            }
          }
          ptr = next_ptr;
          if (ptr) {
            ++i;
          }
        }
        // No success, the path could not be traversed.
        api::error::ErrorPathNotFound error;
        error.path = '/' + current::strings::Join(path, '/');
        error.not_found_component = path[i];
        error.not_found_component_zero_based_index = static_cast<uint32_t>(i);
        return NodeSearchResult(Response(error, HTTPResponseCode.NotFound));
      }
    }
  }

  static NodeSearchResult FindNodeFromWithinTransaction(ImmutableFields<storage_t> fields,
                                                        const std::vector<std::string>& path) {
    return FindNodeFromWithinTransactionImpl(fields, path);
  }

  static NodeSearchResult FindNodeFromWithinTransaction(MutableFields<storage_t> fields,
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
    const auto url_path_args = r.url_path_args;
    const std::vector<std::string> path(url_path_args.begin(), url_path_args.end());
    const bool trailing_slash = r.url_path_had_trailing_slash;
    if (r.method == "GET") {
      storage_
          ->ReadOnlyTransaction(
              [path, trailing_slash](ImmutableFields<storage_t> fields) -> Response {
                const NodeSearchResult result = FindNodeFromWithinTransaction(fields, path);
                if (result.node) {
                  struct GetFileOrDirHandler final {
                    const ImmutableFields<storage_t>& fields;
                    const std::vector<std::string>& path;
                    const bool trailing_slash;
                    GetFileOrDirHandler(const ImmutableFields<storage_t>& fields,
                                        const std::vector<std::string>& path,
                                        const bool trailing_slash)
                        : fields(fields), path(path), trailing_slash(trailing_slash) {}
                    Response response;
                    void FillProtoFields(api::success::FileOrDirResponse& proto_response_object) {
                      proto_response_object.url = "smoke_test_passed://" + current::strings::Join(path, '/');
                      proto_response_object.path = '/' + current::strings::Join(path, '/');
                    }
                    void operator()(const db::File& file) {
                      if (trailing_slash) {
                        response = Response(
                            api::error::Error("TrailingSlashError", "File URLs should not have the trailing slash."),
                            HTTPResponseCode.BadRequest);
                        return;
                      }
                      const auto blob = fields.blob[file.blob];
                      if (Exists(blob)) {
                        api::SuccessfulResponse response_object;
                        auto& file_response_object = response_object.template Construct<api::success::FileResponse>();
                        FillProtoFields(file_response_object);
                        file_response_object.data = Value(blob).body;
                        response = Response(JSON<JSONFormat::JavaScript>(response_object),
                                            HTTPResponseCode.OK,
                                            current::net::constants::kDefaultJSONContentType);
                      } else {
                        response = Response(
                            api::error::Error("InternalFileSystemIntegrityError", "The target blob was not found."),
                            HTTPResponseCode.InternalServerError);
                      }
                    }
                    void operator()(const db::Dir& dir) {
                      api::SuccessfulResponse response_object;
                      auto& dir_response_object = response_object.template Construct<api::success::DirResponse>();
                      FillProtoFields(dir_response_object);
                      for (const auto& e : dir.dir) {
                        const auto node = fields.node[e];
                        if (Exists(node)) {
                          dir_response_object.dir.push_back(Value(node).name);
                        } else {
                          response = Response(api::error::Error("InternalFileSystemIntegrityError",
                                                                "The target node dir refers to a non-existing node."),
                                              HTTPResponseCode.InternalServerError);
                          return;
                        }
                      }
                      response = Response(JSON<JSONFormat::JavaScript>(response_object),
                                          HTTPResponseCode.OK,
                                          current::net::constants::kDefaultJSONContentType);
                    }
                  };
                  GetFileOrDirHandler get_handler(fields, path, trailing_slash);
                  result.node->data.Call(get_handler);
                  return std::move(get_handler.response);
                } else {
                  CURRENT_ASSERT(Exists(result.error));
                  return Value(result.error);
                }
              },
              std::move(r))
          .Go();
    } else if (r.method == "PUT") {
      // TODO(dkorolev): Allow creating directories too.
      // TODO(dkorolev): Overall test that no directory is overwritten with a file or vice versa.
      if (path.empty()) {
        r(api::error::Error("FilesystemError", "Attempted to overwrite the root."), HTTPResponseCode.BadRequest);
        return;
      }
      const auto parsed_body = TryParseJSON<api::request::PutRequest, JSONFormat::Minimalistic>(r.body);
      if (!Exists(parsed_body)) {
        r(api::error::Error("BadRequest",
                            "PUT body should define the creation or the update of a file or of a directory."),
          HTTPResponseCode.BadRequest);
      } else {
        const api::request::PutRequest& body = Value(parsed_body);
        storage_
            ->ReadWriteTransaction(
                [this, path, body, trailing_slash](MutableFields<storage_t> fields) -> Response {
                  struct PutFileOrDirHandler final {
                    const FlowTool* self;
                    MutableFields<storage_t> fields;
                    const std::vector<std::string> path;
                    const bool trailing_slash;
                    Response response;

                    PutFileOrDirHandler(const FlowTool* self,
                                        MutableFields<storage_t> fields,
                                        const std::vector<std::string> path,
                                        const bool trailing_slash)
                        : self(self), fields(fields), path(path), trailing_slash(trailing_slash) {}

                    void operator()(const api::request::PutFileRequest& put_file_request) {
                      if (trailing_slash) {
                        response = Response(
                            api::error::Error("TrailingSlashError", "File URLs should not have the trailing slash."),
                            HTTPResponseCode.BadRequest);
                        return;
                      }
                      const NodeSearchResult full_path_node_search_result =
                          self->FindNodeFromWithinTransaction(fields, path);
                      if (full_path_node_search_result.node) {
                        // A node is found by the full path.
                        // Update the file if it's a file, or return an error if it's a directory.
                        if (Exists<db::File>(full_path_node_search_result.node->data)) {
                          const auto optional_blob =
                              fields.blob[Value<db::File>(full_path_node_search_result.node->data).blob];
                          if (!Exists(optional_blob)) {
                            response = Response(
                                api::error::Error("InternalFileSystemIntegrityError", "The target blob was not found."),
                                HTTPResponseCode.InternalServerError);
                            return;
                          } else {
                            db::Blob blob = Value(optional_blob);
                            blob.body = put_file_request.body;
                            fields.blob.Add(blob);
                            response = "Dima: OK.\n";  // TODO(dkorolev): A better message.
                            return;
                          }
                        } else {
                          response = Response(
                              api::error::Error("FilesystemError", "Attempted to overwrite directory with a file."),
                              HTTPResponseCode.BadRequest);
                          return;
                        }
                      } else {
                        // A node does not exist. See if the directory for it does.
                        const std::string filename = path.back();
                        const std::vector<std::string> dir_path(path.begin(), --path.end());
                        const NodeSearchResult result_dir = self->FindNodeFromWithinTransaction(fields, dir_path);
                        if (result_dir.node) {
                          // Yes, the directory does exist. Create the file.
                          if (Exists<db::Dir>(result_dir.node->data)) {
                            const auto optional_dir_node = fields.node[result_dir.node->key];
                            if (!Exists(optional_dir_node)) {
                              response = Response(api::error::Error("InternalFileSystemIntegrityError",
                                                                    "The assumed directory does not exist."),
                                                  HTTPResponseCode.InternalServerError);
                              return;
                            } else {
                              db::Node dir_node = Value(optional_dir_node);

                              db::Blob file_blob;
                              file_blob.key = db::Blob::GenerateRandomBlobKey();
                              file_blob.body = put_file_request.body;
                              fields.blob.Add(file_blob);

                              db::Node file_node;
                              file_node.key = db::Node::GenerateRandomFileKey();
                              file_node.name = filename;
                              file_node.data = db::File();
                              Value<db::File>(file_node.data).blob = file_blob.key;
                              fields.node.Add(file_node);

                              Value<db::Dir>(dir_node.data).dir.push_back(file_node.key);
                              fields.node.Add(dir_node);

                              response = "Dima: OK.\n";  // TODO(dkorolev): A better message.
                              return;
                            }
                          } else {
                            response = Response(
                                api::error::Error("FilesystemError", "Attempted to use a file as a directory."),
                                HTTPResponseCode.BadRequest);
                            return;
                          }
                        } else {
                          // Neither file nor the directory for it was found.
                          // We don't support `mkdir -p` (yet), so it's an error.
                          CURRENT_ASSERT(Exists(result_dir.error));
                          response = Value(result_dir.error);
                          return;
                        }
                      }
                    }
                    void operator()(const api::request::PutDirRequest&) {
                      const NodeSearchResult full_path_node_search_result =
                          self->FindNodeFromWithinTransaction(fields, path);
                      if (full_path_node_search_result.node) {
                        // A node is found by the full path.
                        // No-op if it's a directory, error if it's a file.
                        if (Exists<db::Dir>(full_path_node_search_result.node->data)) {
                          response = "Dima: OK, dir already existed.\n";  // TODO(dkorolev): A better message.
                        } else {
                          response =
                              Response(api::error::Error("FilesystemError", "Attempted to a file with a directory."),
                                       HTTPResponseCode.BadRequest);
                          return;
                        }
                      } else {
                        // A node does not exist. See if the directory for it does.
                        const std::string dir_name = path.back();
                        const std::vector<std::string> dir_path(path.begin(), --path.end());
                        const NodeSearchResult result_dir = self->FindNodeFromWithinTransaction(fields, dir_path);
                        if (result_dir.node) {
                          // Yes, the directory does exist. Create a subdirectory in it.
                          if (Exists<db::Dir>(result_dir.node->data)) {
                            const auto optional_dir_node = fields.node[result_dir.node->key];
                            if (!Exists(optional_dir_node)) {
                              response = Response(api::error::Error("InternalFileSystemIntegrityError",
                                                                    "The assumed directory does not exist."),
                                                  HTTPResponseCode.InternalServerError);
                              return;
                            } else {
                              db::Node new_dir_node;
                              new_dir_node.key = db::Node::GenerateRandomFileKey();
                              new_dir_node.name = dir_name;
                              new_dir_node.data.template Construct<db::Dir>();
                              fields.node.Add(new_dir_node);

                              db::Node top_level_dir_node = Value(optional_dir_node);
                              Value<db::Dir>(top_level_dir_node.data).dir.push_back(new_dir_node.key);
                              fields.node.Add(top_level_dir_node);

                              response = "Dima: OK, dir created.\n";  // TODO(dkorolev): A better message.
                              return;
                            }
                          } else {
                            response = Response(
                                api::error::Error("FilesystemError", "Attempted to use a file as a directory."),
                                HTTPResponseCode.BadRequest);
                            return;
                          }
                        } else {
                          // Neither file nor the directory for it was found. We don't support `mkdir -p` (yet), so it's
                          // an error.
                          CURRENT_ASSERT(Exists(result_dir.error));
                          response = Value(result_dir.error);
                          return;
                        }
                      }
                    }
                  };
                  PutFileOrDirHandler put_handler(this, fields, path, trailing_slash);
                  body.Call(put_handler);
                  return std::move(put_handler.response);
                },
                std::move(r))
            .Go();
      }
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
