/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef EVENT_STORE_H
#define EVENT_STORE_H

// TODO(dkorolev): Split into test and "live" demo.

#include "../../TypeSystem/struct.h"
#include "../../Storage/storage.h"
#include "../../Storage/persister/sherlock.h"

// TODO(dkorolev): Add HTTP pubsub support.
#include "../../Blocks/HTTP/api.h"

// TODO(dkorolev) + TODO(mzhurovich): Convert all `T_UPPER_CASE` into `upper_case_t`?

template <template <template <typename...> class, template <typename> class, typename>
          class CURRENT_STORAGE_TYPE,
          typename EVENT,
          typename EXTRA_TYPE,
          template <typename...> class DB_PERSISTER>

// NOTE: This class assumes the storage has a Dictionary field called `events`.
struct EventStore final {
  // TODO(dkorolev) + TODO(mzhurovich): All three template parameters here are only to extract `T_TRANSACTION`.
  // Factor it out?
  using transaction_t =
      typename CURRENT_STORAGE_TYPE<SherlockInMemoryStreamPersister,
                                    ::current::storage::transaction_policy::Synchronous,
                                    ::current::storage::persister::NoCustomPersisterParam>::T_TRANSACTION;
  using stream_type_t = Variant<transaction_t, EXTRA_TYPE>;

  using event_store_storage_t =
      CURRENT_STORAGE_TYPE<DB_PERSISTER, current::storage::transaction_policy::Synchronous, stream_type_t>;
  using full_stream_t = typename event_store_storage_t::T_PERSISTER::T_SHERLOCK;

  using nonstorage_stream_t = current::sherlock::Stream<EXTRA_TYPE, current::persistence::Memory>;

  struct ReadonlyStreamFollower {
    using emitter_t = std::function<void(const EXTRA_TYPE&, idxts_t)>;
    const emitter_t emitter;
    explicit ReadonlyStreamFollower(emitter_t emitter) : emitter(emitter) {}
    current::ss::EntryResponse operator()(const EXTRA_TYPE& e, idxts_t current, idxts_t) const {
      emitter(e, current);
      return current::ss::EntryResponse::More;
    }
    static current::ss::TerminationResponse Terminate() { return current::ss::TerminationResponse::Terminate; }
    static current::ss::EntryResponse EntryResponseIfNoMorePassTypeFilter() {
      return current::ss::EntryResponse::More;
    }
  };
  using readonly_stream_follower_t = current::ss::StreamSubscriber<ReadonlyStreamFollower, EXTRA_TYPE>;

  full_stream_t full_event_log;
  nonstorage_stream_t readonly_nonstorage_event_log;
  event_store_storage_t event_store_storage;
  readonly_stream_follower_t readonly_stream_follower;
  typename full_stream_t::template SyncSubscriberScope<readonly_stream_follower_t, EXTRA_TYPE>
      readonly_stream_follower_scope;
  const typename nonstorage_stream_t::T_PERSISTENCE_LAYER& readonly_nonstorage_event_log_persister;
  HTTPRoutesScope http_routes_scope;

  template <typename... ARGS>
  EventStore(int port, const std::string& url_prefix, ARGS&&... args)
      : full_event_log(std::forward<ARGS>(args)...),
        event_store_storage(full_event_log),
        readonly_stream_follower([this](const EXTRA_TYPE& e, idxts_t idx_ts) {
          readonly_nonstorage_event_log.Publish(e, idx_ts.us);
        }),
        readonly_stream_follower_scope(full_event_log.template Subscribe<EXTRA_TYPE>(readonly_stream_follower)),
        readonly_nonstorage_event_log_persister(readonly_nonstorage_event_log.InternalExposePersister()),
        http_routes_scope(HTTP(port).Register(url_prefix + "/up", [](Request r) { r("UP!\n"); }) +
                          HTTP(port).Register(url_prefix + "/event",
                                              URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                              [this](Request r) { EndpointEvent(std::move(r)); })) {}

  ~EventStore() { readonly_stream_follower_scope.Join(); }

  void EndpointEvent(Request r) {
    if (r.method == "GET") {
      if (r.url_path_args.size() != 1u) {
        r("Need one URL parameter.\n", HTTPResponseCode.BadRequest);
      } else {
        const std::string key = r.url_path_args[0];
        event_store_storage.Transaction([key](ImmutableFields<event_store_storage_t> fields) -> Response {
          auto event = fields.events[key];
          // TODO(dkorolev) / TODO(mzhurovich): Why not enable creating `Response` from `ImmutableOptional<T>`,
          // to return { 200, Value() } or a 404?
          if (Exists(event)) {
            return Value(event);
          } else {
            return Response("Not found.\n", HTTPResponseCode.NotFound);
          }
        }, std::move(r)).Wait();
      }
    } else if (r.method == "POST") {
      if (!r.url_path_args.empty()) {
        r("Need no URL parameters.\n", HTTPResponseCode.BadRequest);
      } else {
        try {
          const auto event = ParseJSON<EVENT>(r.body);
          event_store_storage.Transaction([event](MutableFields<event_store_storage_t> fields) -> Response {
            auto existing_event = fields.events[event.key];
            if (Exists(existing_event)) {
              // TODO(dkorolev): Check timestamp? Not now, right. @mzhurovich
              if (JSON(Value(existing_event)) == JSON(event)) {
                return Response("Already published.\n", HTTPResponseCode.OK);
              } else {
                return Response("Conflict, not publishing.\n", HTTPResponseCode.Conflict);
              }
            } else {
              fields.events.Add(event);
              // TODO(dkorolev): Publish a non-Storage record.
              // TODO(dkorolev): Hmm, should it be under a mutex? I vote for a Sherlock-wide mutex. @mzhurovich
              return Response("Created.\n", HTTPResponseCode.Created);
            }
          }, std::move(r)).Wait();
        } catch (const TypeSystemParseJSONException& e) {
          r(std::string("JSON parse error:\n") + e.what() + '\n', HTTPResponseCode.BadRequest);
        }
      }
    } else {
      r("Method not allowed.\n", HTTPResponseCode.MethodNotAllowed);
    }
  }
};

#endif  // EVENT_STORE_H
