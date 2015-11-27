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

#include "schema.h"

#include "../../Sherlock/sherlock.h"
#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/util/random.h"

using namespace current;

DEFINE_bool(legend, true, "Print example usage patterns.");

DEFINE_int32(db_demo_port, 8889, "Local port to spawn the server on.");
DEFINE_string(db_db_dir, ".current", "Local path to the data storage location.");
DEFINE_string(db_db_filename, "data.json", "File name for the persisted data.");

DEFINE_bool(verbose, true, "Dump extra information to stderr.");

DEFINE_double(loadtest,
              0.0,
              "Instead of running the demo, run a load test against the running one for this many seconds.");
DEFINE_string(loadtest_url,
              "http://localhost:%d",
              "The URL for the load test, default to localhost on the right port.");
DEFINE_int32(loadtest_threads, 64, "The number of threads to run requests from.");

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
  std::mutex mutex_;
  const int port_;
  std::atomic_bool replay_done_;

  void NotYetReady(Request r) {
    r(Error("Nicknames read model is still replaying the log. Try again soon."), HTTPResponseCode.BadRequest);
  }

  UserNicknamesReadModel(int port) : port_(port), replay_done_(false) {
    HTTP(port_).Register("/ready",
                         [this](Request r) {
                           if (replay_done_) {
                             r("", HTTPResponseCode.NoContent);
                           } else {
                             NotYetReady(std::move(r));
                           }
                         });
    HTTP(port_).Register("/nickname",
                         [this](Request r) {
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
                         });
  }

  ~UserNicknamesReadModel() {
    HTTP(port_).UnRegister("/nickname");
    HTTP(port_).UnRegister("/ready");
  }

  // A new event from the event log. Process it in a polymorphic way, ignore everything but `UserAdded`.
  void operator()(const Event& e) {
    std::lock_guard<std::mutex> lock(mutex_);
    e.event.Call(state_);
  }

  // Once the batch replay is done, keep running assuming eventual consistency.
  void ReplayDone() {
    if (FLAGS_verbose) {
      std::cerr << "Replay done, ready to serve the read model in eventually consistent fashion." << std::endl;
    }
    replay_done_ = true;
  }
};

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (FLAGS_loadtest) {
    class Worker {
     public:
      Worker(int port, double seconds)
          : port_(port), seconds_to_run_(seconds), queries_(0u), thread_(&Worker::Thread, this) {}
      void Join() { thread_.join(); }
      size_t TotalQueries() const { return queries_; }

     private:
      static double NowInSeconds() { return 1e-6 * static_cast<double>(EpochMicroseconds(time::Now()).us); }
      void Thread() {
        const std::string url = strings::Printf(FLAGS_loadtest_url.c_str(), port_) + "/publish";
        const Event body = []() {
          Event event;
          event.timestamp = EpochMicroseconds(time::Now()).us;
          UserAdded body;
          body.user_id = std::string(' ', 10);
          body.nickname = std::string(' ', 4);
          for (auto& c : body.user_id) {
            c = random::RandomIntegral<char>('a', 'z');
          }
          for (auto& c : body.nickname) {
            c = random::RandomIntegral<char>('A', 'Z');
          }
          event.event = body;
          return event;
        }();
        const double timestamp_begin = NowInSeconds();
        const double timestamp_end = timestamp_begin + seconds_to_run_;
        double timestamp_now;
        while ((timestamp_now = NowInSeconds()) < timestamp_end) {
          assert(HTTP(POST(url, body)).code == HTTPResponseCode.NoContent);
          ++queries_;
        }
      }
      const int port_;
      const double seconds_to_run_;
      size_t queries_;
      std::thread thread_;
    };
    std::vector<std::unique_ptr<Worker>> threads(FLAGS_loadtest_threads);
    for (auto& t : threads) {
      t = make_unique<Worker>(FLAGS_db_demo_port, FLAGS_loadtest);
    }
    for (auto& t : threads) {
      t->Join();
    }
    size_t total_queries = 0u;
    for (auto& t : threads) {
      total_queries += t->TotalQueries();
    }
    std::cout << "QPS: " << std::setw(3) << (total_queries / FLAGS_loadtest) << std::endl;
    return 0;
  }

  auto stream = sherlock::Stream<Event, blocks::persistence::NewAppendToFile>(
      "persistence", FileSystem::JoinPath(FLAGS_db_db_dir, FLAGS_db_db_filename));

  // Example command lines to get started.
  if (FLAGS_legend) {
    const auto url = "localhost:" + strings::ToString(FLAGS_db_demo_port);

    std::cerr << "Health check:\n\tcurl " << url << "/healthz" << std::endl;
    std::cerr << "Schema as F#:\n\tcurl " << url << "/schema.fs" << std::endl;
    std::cerr << "Schema as C++:\n\tcurl " << url << "/schema.h" << std::endl;
    std::cerr << "Event log, structured, persistent:"
              << "\n\tcurl " << url << "/data" << std::endl;

    {
      Event event;
      event.timestamp = EpochMicroseconds(time::Now()).us;
      UserAdded body;
      body.user_id = "skywalker";
      body.nickname = "Luke";
      event.event = body;
      std::cerr << "Example publish:\n\tcurl -d '" << JSON(event) << "' " << url << "/publish" << std::endl;
    }

    std::cerr << "Read model:\n\tcurl '" << url << "/nickname?id=skywalker'" << std::flush << std::endl;
  } else {
    std::cerr << "Demo Embedded Event Log DB running on localhost:" << FLAGS_db_demo_port << std::flush
              << std::endl;
  }

  HTTP(FLAGS_db_demo_port).Register("/healthz", [](Request r) { r("OK\n"); });

  // Schema.
  using reflection::SchemaInfo;
  using reflection::StructSchema;
  using reflection::Language;

  HTTP(FLAGS_db_demo_port)
      .Register("/schema.h",
                [](Request r) {
                  StructSchema schema;
                  schema.AddType<Event>();
                  r(schema.Describe(Language::CPP()), HTTPResponseCode.OK, "text/plain; charset=us-ascii");
                });

  HTTP(FLAGS_db_demo_port)
      .Register("/schema.fs",
                [](Request r) {
                  StructSchema schema;
                  schema.AddType<Event>();
                  r(schema.Describe(Language::FSharp()), HTTPResponseCode.OK, "text/plain; charset=us-ascii");
                });

  HTTP(FLAGS_db_demo_port)
      .Register("/schema.json",
                [](Request r) {
                  StructSchema schema;
                  schema.AddType<Event>();
                  r(schema.GetSchemaInfo());
                });

  // Subscribe.
  HTTP(FLAGS_db_demo_port).Register("/data", stream);

  // Publish.
  HTTP(FLAGS_db_demo_port)
      .Register("/publish",
                [&stream](Request r) {
                  if (r.method == "POST") {
                    try {
                      auto event = ParseJSON<Event>(r.body);
                      stream.Publish(std::move(event));
                      r("", HTTPResponseCode.NoContent);
                    } catch (const Exception& e) {
                      r(Error(e.What()), HTTPResponseCode.BadRequest);
                    }
                  } else {
                    r(Error("This request should be a POST."), HTTPResponseCode.MethodNotAllowed);
                  }
                });

  // Read model.
  stream.AsyncSubscribe(make_unique<UserNicknamesReadModel>(FLAGS_db_demo_port)).Detach();

  // Run forever.
  HTTP(FLAGS_db_demo_port).Join();
}
