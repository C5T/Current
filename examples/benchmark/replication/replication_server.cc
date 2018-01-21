/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2017 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#include "../../../blocks/http/api.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../stream/stream.h"

#include "generate_stream.h"

DEFINE_string(route, "/raw_log", "Route to spawn the stream on.");
DEFINE_uint16(port, 8383, "Port to spawn the stream on.");
DEFINE_string(stream_data_filename,
              "",
              "If set, path to load the stream data from. If not, the data is generated from scratch.");
DEFINE_uint32(entry_length, 1000, "The length of the string member values in the generated stream entries.");
DEFINE_uint32(entries_count, 10000, "The number of entries to replicate.");
DEFINE_bool(use_fake_stream, false, "Set to use fake stream, that just sends predefined data.");
DEFINE_uint32(fake_stream_chunk_size,
              100 * 1024,
              "The size (in bytes) of portions by which the predefined data is sent (if use_fake_stream flag is set).");
DEFINE_bool(do_not_remove_autogen_data, false, "Set to not remove the data file.");

class FakeStream final {
 public:
  struct FakeStreamData final {
    explicit FakeStreamData(std::string&& data) : data(std::move(data)) {}

    std::string data;
    mutable std::mutex data_mutex;

    using http_subscriptions_t = std::unordered_map<
        std::string,
        std::pair<current::stream::SubscriberScope, std::unique_ptr<current::stream::AbstractSubscriberObject>>>;
    mutable std::mutex http_subscriptions_mutex;
    mutable http_subscriptions_t http_subscriptions;
  };
  using impl_t = FakeStreamData;
  using entry_t = benchmark::replication::Entry;

  class FakePubSubEndpoint : public current::stream::AbstractSubscriberObject {
   public:
    FakePubSubEndpoint(const std::string& subscription_id,
                       current::Borrowed<impl_t> data,
                       uint64_t stream_size,
                       Request r)
        : impl_(std::move(data), [this]() { termination_requested_ = true; }),
          http_request_(std::move(r)),
          http_response_(http_request_.SendChunkedResponse(
              HTTPResponseCode.OK,
              current::net::constants::kDefaultJSONContentType,
              current::net::http::Headers({
                  {current::stream::kStreamHeaderCurrentSubscriptionId, subscription_id},
                  {current::stream::kStreamHeaderCurrentStreamSize, current::ToString(stream_size)},
              }))) {}

    current::ss::EntryResponse operator()(std::string&& data) {
      const current::ss::EntryResponse result = [&, this]() {
        if (termination_requested_) {
          return current::ss::EntryResponse::Done;
        }
        try {
          http_response_(std::move(data));
        } catch (const current::net::NetworkException&) {  // LCOV_EXCL_LINE
          return current::ss::EntryResponse::Done;         // LCOV_EXCL_LINE
        }
        return current::ss::EntryResponse::More;
      }();
      if (result == current::ss::EntryResponse::Done) {
        // flush cached response data.
        http_response_("");
      }
      return result;
    }

    // LCOV_EXCL_START
    current::ss::TerminationResponse Terminate() {
      static const std::string message = "{\"error\":\"The subscriber has terminated.\"}\n";
      http_response_(message);
      return current::ss::TerminationResponse::Terminate;
    }
    // LCOV_EXCL_STOP

   private:
    // The HTTP listener must register itself as a user of stream data to ensure the lifetime of stream data.
    const current::BorrowedWithCallback<impl_t> impl_;
    std::atomic_bool termination_requested_{false};

    // `http_request_`:  need to keep the passed in request in scope for the lifetime of the chunked response.
    Request http_request_;
    // `http_response_`: the instance of the chunked response object to use.
    current::net::HTTPServerConnection::ChunkedResponseSender<CURRENT_BRICKS_HTTP_DEFAULT_CHUNK_CACHE_SIZE>
        http_response_;

    FakePubSubEndpoint() = delete;
    FakePubSubEndpoint(const FakePubSubEndpoint&) = delete;
    void operator=(const FakePubSubEndpoint&) = delete;
    FakePubSubEndpoint(FakePubSubEndpoint&&) = delete;
    void operator=(FakePubSubEndpoint&&) = delete;
  };

  explicit FakeStream(const std::string& filename)
      : schema_as_object_(benchmark::replication::stream_t::StaticConstructSchemaAsObject(schema_namespace_name_)),
        impl_(current::MakeOwned<impl_t>(current::FileSystem::ReadFileAsString(filename))){};

  void operator()(Request r) {
    if (r.url.query.has("json")) {
      const auto& json = r.url.query["json"];
      if (json == "minimalistic") {
        ServeDataViaHTTP<JSONFormat::Minimalistic>(std::move(r));
      } else if (json == "js") {
        ServeDataViaHTTP<JSONFormat::JavaScript>(std::move(r));
      } else if (json == "fs") {
        ServeDataViaHTTP<JSONFormat::NewtonsoftFSharp>(std::move(r));
      } else {
        r("The `?json` parameter is invalid, legal values are `minimalistic`, `js`, `fs`, or omit the parameter.\n",
          HTTPResponseCode.NotFound);
      }
    } else {
      ServeDataViaHTTP<JSONFormat::Current>(std::move(r));
    }
  }

 private:
  template <typename F>
  class SubscriberThreadInstance final : public current::stream::SubscriberScope::SubscriberThread {
   private:
    bool this_is_valid_;
    std::function<void()> done_callback_;
    current::WaitableTerminateSignal terminate_signal_;
    bool terminate_sent_;
    current::BorrowedWithCallback<impl_t> impl_;
    F& subscriber_;
    std::thread thread_;

    SubscriberThreadInstance() = delete;
    SubscriberThreadInstance(const SubscriberThreadInstance&) = delete;
    SubscriberThreadInstance(SubscriberThreadInstance&&) = delete;
    void operator=(const SubscriberThreadInstance&) = delete;
    void operator=(SubscriberThreadInstance&&) = delete;

   public:
    SubscriberThreadInstance(current::Borrowed<impl_t> impl, F& subscriber, std::function<void()> done_callback)
        : this_is_valid_(false),
          done_callback_(done_callback),
          terminate_signal_(),
          terminate_sent_(false),
          impl_(std::move(impl),
                [this]() {
                  std::lock_guard<std::mutex> lock(impl_->data_mutex);
                  terminate_signal_.SignalExternalTermination();
                }),
          subscriber_(subscriber),
          thread_(&SubscriberThreadInstance::Thread, this) {
      this_is_valid_ = true;
    }

    ~SubscriberThreadInstance() {
      if (this_is_valid_) {
        CURRENT_ASSERT(thread_.joinable());
        if (!subscriber_thread_done_) {
          std::lock_guard<std::mutex> lock(impl_->data_mutex);
          terminate_signal_.SignalExternalTermination();
        }
        thread_.join();
      } else {
        // The constructor has not completed successfully. The thread was not started, and `impl_` is garbage.
        if (done_callback_) {
          done_callback_();
        }
      }
    }

    void Thread() {
      ThreadImpl();
      subscriber_thread_done_ = true;
      std::lock_guard<std::mutex> lock(impl_->http_subscriptions_mutex);
      if (done_callback_) {
        done_callback_();
      }
    }

    void ThreadImpl() {
      uint64_t offset = 0;
      while (offset < impl_->data.size() &&
             impl_->data[offset] == current::persistence::impl::constants::kDirectiveMarker) {
        offset = impl_->data.find('\n', offset) + 1;
      }
      while (offset < impl_->data.size()) {
        if (!terminate_sent_ && terminate_signal_) {
          terminate_sent_ = true;
          return;
        }
        const uint64_t size = offset < impl_->data.size() + FLAGS_fake_stream_chunk_size ? FLAGS_fake_stream_chunk_size
                                                                                         : impl_->data.size() - offset;
        subscriber_(impl_->data.substr(offset, size));
        offset += size;
      }
    }
  };

  template <typename F>
  class SubscriberScope final : public current::stream::SubscriberScope {
   private:
    using base_t = current::stream::SubscriberScope;

   public:
    using subscriber_thread_t = SubscriberThreadInstance<F>;

    SubscriberScope(current::Borrowed<impl_t> impl, F& subscriber, std::function<void()> done_callback)
        : base_t(std::move(std::make_unique<subscriber_thread_t>(std::move(impl), subscriber, done_callback))) {}

    SubscriberScope(SubscriberScope&&) = default;
    SubscriberScope& operator=(SubscriberScope&&) = default;

    SubscriberScope() = delete;
    SubscriberScope(const SubscriberScope&) = delete;
    SubscriberScope& operator=(const SubscriberScope&) = delete;
  };

  template <typename F>
  current::stream::SubscriberScope Subscribe(F& subscriber, std::function<void()> done_callback) const {
    return SubscriberScope<F>(impl_, subscriber, done_callback);
  }

  template <class J>
  void ServeDataViaHTTP(Request r) const {
    if (r.method != "GET" && r.method != "HEAD") {
      r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed);
      return;
    }

    const current::Borrowed<impl_t> borrowed_impl(impl_);
    if (!borrowed_impl) {
      r("", HTTPResponseCode.ServiceUnavailable);
      return;
    }

    auto request_params = current::stream::ParsePubSubHTTPRequest(r);

    if (request_params.terminate_requested) {
      typename impl_t::http_subscriptions_t::iterator it;
      {
        std::lock_guard<std::mutex> lock(borrowed_impl->http_subscriptions_mutex);
        it = borrowed_impl->http_subscriptions.find(request_params.terminate_id);
      }
      if (it != borrowed_impl->http_subscriptions.end()) {
        it->second.first = nullptr;
        r("", HTTPResponseCode.OK);
      } else {
        r("", HTTPResponseCode.NotFound);
      }
      return;
    }

    const auto stream_size = FLAGS_entries_count;

    if (request_params.size_only) {
      const std::string size_str = current::ToString(stream_size);
      const std::string body = (r.method == "GET") ? size_str + '\n' : "";
      r(body,
        HTTPResponseCode.OK,
        current::net::constants::kDefaultContentType,
        current::net::http::Headers({{current::stream::kStreamHeaderCurrentStreamSize, size_str}}));
      return;
    }

    if (request_params.schema_requested) {
      const std::string& schema_format = request_params.schema_format;
      // Return the schema the user is requesting, in a top-level, or more fine-grained format.
      if (schema_format.empty()) {
        r(schema_as_http_response_);
      } else if (schema_format == "simple") {
        r(current::stream::SubscribableStreamSchema(
            schema_as_object_.type_id, schema_namespace_name_.entry_name, schema_namespace_name_.namespace_name));
      } else {
        const auto cit = schema_as_object_.language.find(schema_format);
        if (cit != schema_as_object_.language.end()) {
          r(cit->second);
        } else {
          current::stream::StreamSchemaFormatNotFoundError four_oh_four;
          four_oh_four.unsupported_format_requested = schema_format;
          r(four_oh_four, HTTPResponseCode.NotFound);
        }
      }
    } else {
      const std::string subscription_id = benchmark::replication::stream_t::GenerateRandomHTTPSubscriptionID();

      auto http_chunked_subscriber =
          std::make_unique<FakePubSubEndpoint>(subscription_id, borrowed_impl, stream_size, std::move(r));

      const auto done_callback = [this, borrowed_impl, subscription_id]() {
        // Note: Called from a locked section of `borrowed_impl->http_subscriptions_mutex`.
        borrowed_impl->http_subscriptions[subscription_id].second = nullptr;
      };
      current::stream::SubscriberScope http_chunked_subscriber_scope =
          Subscribe(*http_chunked_subscriber, done_callback);

      {
        std::lock_guard<std::mutex> lock(borrowed_impl->http_subscriptions_mutex);
        // Note: Silently relying on no collision here. Suboptimal.
        if (!borrowed_impl->http_subscriptions.count(subscription_id)) {
          borrowed_impl->http_subscriptions[subscription_id] =
              std::make_pair(std::move(http_chunked_subscriber_scope), std::move(http_chunked_subscriber));
        }
      }
    }
  }

 private:
  const current::ss::StreamNamespaceName schema_namespace_name_ = current::ss::StreamNamespaceName(
      current::stream::constants::kDefaultNamespaceName, current::stream::constants::kDefaultTopLevelName);
  const current::stream::StreamSchema schema_as_object_;

  const Response schema_as_http_response_ = Response(JSON<JSONFormat::Minimalistic>(schema_as_object_),
                                                     HTTPResponseCode.OK,
                                                     current::net::constants::kDefaultJSONContentType);

  current::Owned<impl_t> impl_;
};

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  Optional<current::Owned<benchmark::replication::stream_t>> stream;
  std::unique_ptr<current::FileSystem::ScopedRmFile> temp_file_remover;
  std::unique_ptr<FakeStream> fake_stream;
  auto filename = FLAGS_stream_data_filename;
  if (filename.empty()) {
    filename = current::FileSystem::GenTmpFileName();
    std::cout << "Generating " << filename << " with " << FLAGS_entries_count << " entries of " << FLAGS_entry_length
              << " bytes each." << std::endl;
    if (!FLAGS_do_not_remove_autogen_data) {
      temp_file_remover = std::make_unique<current::FileSystem::ScopedRmFile>(filename);
    }
    benchmark::replication::GenerateStream(filename, FLAGS_entry_length, FLAGS_entries_count, stream);
  } else if (!FLAGS_use_fake_stream) {
    stream = benchmark::replication::stream_t::CreateStream(filename);
  }
  if (FLAGS_use_fake_stream) {
    stream = nullptr;
    fake_stream = std::make_unique<FakeStream>(filename);
  }
  std::cout << "Spawning the" << (FLAGS_use_fake_stream ? " fake " : " ") << "server on port " << FLAGS_port
            << std::endl;
  const auto scope =
      FLAGS_use_fake_stream
          ? HTTP(FLAGS_port)
                .Register(FLAGS_route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *fake_stream)
          : HTTP(FLAGS_port)
                .Register(FLAGS_route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *Value(stream));
  std::cout << "The server is up on http://localhost:" << FLAGS_port << FLAGS_route << std::endl;
  HTTP(FLAGS_port).Join();
  return 0;
}
