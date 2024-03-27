/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../../current.h"

#include "schema.h"

using namespace current;

DEFINE_string(db_dir, ".current", "Local path to the data storage location.");
DEFINE_string(db_filename, "data.json", "File name for the persisted data.");

DEFINE_bool(legend, true, "Print example usage patterns.");

DEFINE_bool(verbose, true, "Dump extra information to stderr.");

// Example eventually consistent read model.
struct UserNicknamesReadModel {
  // The in-memory state and exclusive operations on it.
  struct State {
    // The mutable state.
    std::unordered_map<std::string, std::string> nicknames;

    // The mutator.
    void operator()(const UserAdded& user) { nicknames[user.user_id] = user.nickname; }

    // Ignore all other types of events.
    void operator()(const CurrentSuper&) {}
  };

  // Mutex-locked state for eventually consistent read model.
  State state_;
  mutable std::mutex mutex_;
  std::atomic_bool replay_done_;
  HTTPRoutesScope endpoints_;

  UserNicknamesReadModel(int port)
      : replay_done_(false),
        endpoints_(HTTP(port).Register("/ready", [this](Request r) { OnReady(std::move(r)); }) +
                   HTTP(port).Register("/nickname", [this](Request r) { OnNickname(std::move(r)); })) {}

  void OnReady(Request r) const {
    if (replay_done_) {
      r("", HTTPResponseCode.NoContent);
    } else {
      NotYetReady(std::move(r));
    }
  }

  void OnNickname(Request r) const {
    if (replay_done_) {
      const std::string user_id = r.url.query["id"];
      std::lock_guard<std::mutex> lock(mutex_);
      const auto cit = state_.nicknames.find(user_id);
      if (cit != state_.nicknames.end()) {
        r(UserNickname(user_id, cit->second));
      } else {
        r(UserNicknameNotFound(user_id), HTTPResponseCode.NotFound);
      }
    } else {
      NotYetReady(std::move(r));
    }
  }

  void NotYetReady(Request r) const {
    r(Error("Nicknames read model is still replaying the log. Try again soon."), HTTPResponseCode.BadRequest);
  }

  // A new event from the event log. Process it in a polymorphic way, ignore everything but `UserAdded`.
  current::ss::EntryResponse operator()(const Event& e, idxts_t current, idxts_t total) {
    std::lock_guard<std::mutex> lock(mutex_);
    e.event.Call(state_);
    if (current.index + 1 == total.index) {
      replay_done_ = true;
    }
    return current::ss::EntryResponse::More;
  }
  current::ss::EntryResponse operator()(std::chrono::microseconds) const { return current::ss::EntryResponse::More; }

  current::ss::EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return current::ss::EntryResponse::More; }

  current::ss::TerminationResponse Terminate() { return current::ss::TerminationResponse::Terminate; }
};

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  // NOTE(dkorolev): The below code is wrong as it doesn't save the scope of HTTP handlers, duh.
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  auto stream = current::stream::Stream<Event, current::persistence::File>::CreateStream(
      FileSystem::JoinPath(FLAGS_db_dir, FLAGS_db_filename));

  // Example command lines to get started.
  if (FLAGS_legend) {
    const auto url = "localhost:" + current::ToString(port);

    std::cerr << "Health check:\n\tcurl " << url << "/healthz" << std::endl;
    std::cerr << "Schema as F#:\n\tcurl " << url << "/schema.fs" << std::endl;
    std::cerr << "Schema as C++:\n\tcurl " << url << "/schema.h" << std::endl;
    std::cerr << "Event log, structured, persistent:"
              << "\n\tcurl " << url << "/data" << std::endl;

    {
      UserAdded body;
      body.timestamp = time::Now();
      body.user_id = "skywalker";
      body.nickname = "Luke";
      Event event(body);
      std::cerr << "Example publish (timestamp will be overwritten):\n"
                << "\tcurl -d '" << JSON(event) << "' " << url << "/publish" << std::endl;
    }

    std::cerr << "Read model:\n\tcurl '" << url << "/nickname?id=skywalker'" << std::flush << std::endl;
  } else {
    std::cerr << "Demo Embedded Event Log DB running on localhost:" << port << std::flush << std::endl;
  }

  HTTPRoutesScope scope = HTTP(port).Register("/healthz", [](Request r) { r("OK\n"); });

  // Schema.
  using reflection::Language;
  using reflection::SchemaInfo;
  using reflection::StructSchema;

  scope += HTTP(port).Register("/schema.h", [](Request r) {
    StructSchema schema;
    schema.AddType<Event>();
    r(schema.GetSchemaInfo().Describe<Language::CPP>(), HTTPResponseCode.OK, "text/plain; charset=us-ascii");
  });

  scope += HTTP(port).Register("/schema.fs", [](Request r) {
    StructSchema schema;
    schema.AddType<Event>();
    r(schema.GetSchemaInfo().Describe<Language::FSharp>(), HTTPResponseCode.OK, "text/plain; charset=us-ascii");
  });

  scope += HTTP(port).Register("/schema.json", [](Request r) {
    StructSchema schema;
    schema.AddType<Event>();
    r(schema.GetSchemaInfo());
  });

  // Subscribe.
  scope += HTTP(port).Register("/data", *stream);

  // Publish.
  scope += HTTP(port).Register("/publish", [&stream](Request r) {
    if (r.method == "POST") {
      try {
        auto event = ParseJSON<Event>(r.body);
        SetMicroTimestamp(event, time::Now());
        stream->Publisher()->Publish(std::move(event));
        r("", HTTPResponseCode.NoContent);
      } catch (const Exception& e) {
        r(Error(e.DetailedDescription()), HTTPResponseCode.BadRequest);
      }
    } else {
      r(Error("This request should be a POST."), HTTPResponseCode.MethodNotAllowed);
    }
  });

  // Read model.
  current::ss::StreamSubscriber<UserNicknamesReadModel, Event> read_model(port);
  const auto subscriber_scope = stream->Subscribe(read_model);

  // Run forever.
  HTTP(port).Join();
}
