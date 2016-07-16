#include <atomic>

#include "../../../Bricks/dflags/dflags.h"

#include "schema.h"

DEFINE_string(file, ".current/log.json", "Storage persistence file in Sherlock format to use.");
DEFINE_uint32(gen, 0u, "Set to nonzero to generate this number of entries, overwriting the test data.");
DEFINE_int16(sleep_ms,
             10,
             "The time in milliseconds to sleep in the spin-lock while checking "
             "actual replayed entries count.");
DEFINE_uint16(subs, 0, "The number of dummy stream subscribers.");

inline void GenerateTestData(const std::string& file, uint32_t size) {
  current::FileSystem::RmFile(file, current::FileSystem::RmFileParameters::Silent);
  storage_t storage(file);
  for (uint32_t i = 0u; i < size; ++i) {
    storage.ReadWriteTransaction([i](MutableFields<storage_t> fields) {
      Entry entry(static_cast<EntryID>(i), current::SHA256(current::ToString(i)));
      fields.entries.Add(entry);
    }).Go();
  }
}

template <typename T>
struct RawLogSubscriberImpl {
  struct MutationCollector {
    void operator()(const EntryDictUpdated& e) { crc_ ^= current::CRC32(Value<Entry>(e.data).value); }

    template <typename E>
    void operator()(const E&) const {}

    uint32_t crc_ = 0u;
  };

  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;

  RawLogSubscriberImpl() : entries_seen_(0u) {}

  EntryResponse operator()(const T& transaction, idxts_t, idxts_t) {
    for (const auto& m : transaction.mutations) {
      m.Call(mutation_collector_);
    }
    ++entries_seen_;
    return EntryResponse::More;
  }

  TerminationResponse Terminate() {
    terminating_ = true;
    return TerminationResponse::Terminate;
  }

  EntryResponse EntryResponseIfNoMorePassTypeFilter() const {
    if (terminating_) {
      return EntryResponse::Done;
    } else {
      return EntryResponse::More;
    }
  }

  std::atomic<uint64_t> entries_seen_;
  bool terminating_ = false;
  MutationCollector mutation_collector_;
};

inline void PerformReplayBenchmark(const std::string& file,
                                   std::chrono::milliseconds spin_lock_sleep,
                                   uint16_t subscribers_count) {
  using stream_t = typename storage_t::persister_t::sherlock_t;
  using transaction_t = typename stream_t::entry_t;
  using subscriber_t = current::ss::StreamSubscriber<RawLogSubscriberImpl<transaction_t>, transaction_t>;

  // Owning storage replay.
  {
    std::cout << "=== Owning storage ===" << std::endl;
    const auto begin = current::time::Now();
    stream_t stream(file);
    const auto stream_initialized = current::time::Now();
    std::vector<subscriber_t> subscribers(subscribers_count);
    std::vector<current::sherlock::SubscriberScope> sub_scopes;
    if (subscribers_count) {
      for (size_t i = 0u; i < subscribers_count; ++i) {
        sub_scopes.emplace_back(std::move(stream.Subscribe(subscribers[i])));
      }
    }
    const auto subscribers_created = current::time::Now();
    storage_t storage(stream);
    const auto end = current::time::Now();
    std::cout << "* Stream initialization: " << (stream_initialized - begin).count() / 1000 << " ms\n";
    if (subscribers_count) {
      std::cout << "* Spawning subscribers: " << (subscribers_created - stream_initialized).count() / 1000
                << " ms\n";
    }
    std::cout << "* Storage replay: " << (end - subscribers_created).count() / 1000 << " ms" << std::endl;
  }

  struct PublisherAcquirer {
    using publisher_t = typename stream_t::publisher_t;
    void AcceptPublisher(std::unique_ptr<publisher_t> publisher) { publisher_ = std::move(publisher); }
    std::unique_ptr<publisher_t> publisher_;
  };

  // Non-owning storage replay.
  {
    std::cout << "=== Non-owning storage ===" << std::endl;
    const auto begin = current::time::Now();
    stream_t stream(file);
    PublisherAcquirer acquirer;
    stream.MovePublisherTo(acquirer);
    const uint64_t stream_size = stream.InternalExposePersister().Size();
    const auto stream_initialized = current::time::Now();
    std::vector<subscriber_t> subscribers(subscribers_count);
    std::vector<current::sherlock::SubscriberScope> sub_scopes;
    if (subscribers_count) {
      for (size_t i = 0u; i < subscribers_count; ++i) {
        sub_scopes.emplace_back(std::move(stream.Subscribe(subscribers[i])));
      }
    }
    const auto subscribers_created = current::time::Now();
    uint64_t storage_size = 0u;
    storage_t storage(stream);
    // Spin lock checking the number of imported entries.
    while (storage_size < stream_size) {
      storage.ReadOnlyTransaction([&storage_size](ImmutableFields<storage_t> fields) {
        storage_size = fields.entries.Size();
      }).Go();
      std::this_thread::sleep_for(spin_lock_sleep);
    }
    const auto end = current::time::Now();
    std::cout << "* Stream initialization: " << (stream_initialized - begin).count() / 1000 << " ms\n";
    if (subscribers_count) {
      std::cout << "* Spawning subscribers: " << (subscribers_created - stream_initialized).count() / 1000
                << " ms\n";
    }
    std::cout << "* Storage replay: " << (end - subscribers_created).count() / 1000 << " ms" << std::endl;
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  if (FLAGS_gen) {
    GenerateTestData(FLAGS_file, FLAGS_gen);
  } else {
    PerformReplayBenchmark(FLAGS_file, std::chrono::milliseconds(FLAGS_sleep_ms), FLAGS_subs);
  }
}
