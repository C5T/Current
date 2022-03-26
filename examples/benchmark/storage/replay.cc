#include <atomic>

#include "schema.h"

#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/graph/gnuplot.h"

DEFINE_string(file, ".current/log.json", "Storage persistence file in Stream format to use.");
DEFINE_uint32(gen, 0u, "Set to nonzero to generate this number of entries, overwriting the test data.");
DEFINE_int16(sleep_ms,
             10,
             "The time in milliseconds to sleep in the spin-lock while checking "
             "actual replayed entries count.");
DEFINE_uint16(subs, 0, "The number of dummy stream subscribers.");

DEFINE_uint16(subs_range, 25, "For batch test, iterate from zero to this number of subscribers, inclusive.");
DEFINE_string(json, ".current/result.json", "The name of the file to write the benchmark result as JSON.");
DEFINE_string(png, ".current/result.png", "The name of the file to write the benchmark resuls as PNG.");

inline void GenerateTestData(const std::string& file, uint32_t size) {
  current::FileSystem::RmFile(file, current::FileSystem::RmFileParameters::Silent);
  auto storage = storage_t::CreateMasterStorage(file);
  for (uint32_t i = 0u; i < size; ++i) {
    storage
        ->ReadWriteTransaction([i](MutableFields<storage_t> fields) {
          Entry entry(static_cast<EntryID>(i), current::SHA256(current::ToString(i)));
          fields.entries.Add(entry);
        })
        .Go();
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

  EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

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

CURRENT_STRUCT(Report) {
  CURRENT_FIELD(subs, std::vector<uint16_t>);
  CURRENT_FIELD(owning_storage_replay_ms, std::vector<uint64_t>);
  CURRENT_FIELD(following_storage_replay_ms, std::vector<uint64_t>);
  CURRENT_FIELD(raw_persister_replay_ms, std::vector<uint64_t>);
  CURRENT_FIELD(raw_file_scan_ms, std::vector<uint64_t>);
  CURRENT_FIELD(parsing_file_scan_ms, std::vector<uint64_t>);
};

inline void PerformReplayBenchmark(const std::string& file,
                                   std::chrono::milliseconds spin_lock_sleep,
                                   uint16_t subscribers_count,
                                   Report& report) {
  using stream_t = typename storage_t::persister_t::stream_t;
  using transaction_t = typename stream_t::entry_t;
  using subscriber_t = current::ss::StreamSubscriber<RawLogSubscriberImpl<transaction_t>, transaction_t>;

  report.subs.push_back(subscribers_count);

  // Owning storage replay.
  {
    std::cout << "=== Owning storage ===" << std::endl;
    const auto begin = current::time::Now();
    auto stream = stream_t::CreateStream(file);
    const auto stream_initialized = current::time::Now();
    std::vector<subscriber_t> subscribers(subscribers_count);
    std::vector<current::stream::SubscriberScope> sub_scopes;
    if (subscribers_count) {
      for (size_t i = 0u; i < subscribers_count; ++i) {
        sub_scopes.emplace_back(stream->Subscribe(subscribers[i]));
      }
    }
    const auto subscribers_created = current::time::Now();
    auto storage = storage_t::CreateMasterStorageAtopExistingStream(stream);
    const auto end = current::time::Now();
    std::cout << "* Stream initialization: " << (stream_initialized - begin).count() / 1000 << " ms\n";
    if (subscribers_count) {
      std::cout << "* Spawning subscribers: " << (subscribers_created - stream_initialized).count() / 1000 << " ms\n";
    }
    std::cout << "* Storage replay: " << (end - subscribers_created).count() / 1000 << " ms" << std::endl;
    report.owning_storage_replay_ms.push_back((end - subscribers_created).count() / 1000);
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
    auto stream = stream_t::CreateStream(file);
    auto acquired_publisher = stream->BecomeFollowingStream();
    const uint64_t stream_size = stream->Data()->Size();
    const auto stream_initialized = current::time::Now();
    std::vector<subscriber_t> subscribers(subscribers_count);
    std::vector<current::stream::SubscriberScope> sub_scopes;
    if (subscribers_count) {
      for (size_t i = 0u; i < subscribers_count; ++i) {
        sub_scopes.emplace_back(stream->Subscribe(subscribers[i]));
      }
    }
    const auto subscribers_created = current::time::Now();
    uint64_t storage_size = 0u;
    auto storage = storage_t::CreateFollowingStorageAtopExistingStream(stream);
    // Spin lock checking the number of imported entries.
    while (storage_size < stream_size) {
      storage
          ->ReadOnlyTransaction(
              [&storage_size](ImmutableFields<storage_t> fields) { storage_size = fields.entries.Size(); })
          .Go();
      std::this_thread::sleep_for(spin_lock_sleep);
    }
    const auto end = current::time::Now();
    std::cout << "* Stream initialization: " << (stream_initialized - begin).count() / 1000 << " ms\n";
    if (subscribers_count) {
      std::cout << "* Spawning subscribers: " << (subscribers_created - stream_initialized).count() / 1000 << " ms\n";
    }
    std::cout << "* Storage replay: " << (end - subscribers_created).count() / 1000 << " ms" << std::endl;
    report.following_storage_replay_ms.push_back((end - subscribers_created).count() / 1000);

    // Bare persister iteration.
    {
      std::vector<std::thread> threads(subscribers_count);
      const auto& persister = stream->Data();
      const auto begin = current::time::Now();
      for (auto& t : threads) {
        t = std::thread([&]() {
          for (const auto& e : persister->Iterate(0, stream_size)) {
            CURRENT_ASSERT(e.entry.mutations.size() == 1u);
          }
        });
      }
      for (auto& t : threads) {
        t.join();
      }
      const auto end = current::time::Now();
      report.raw_persister_replay_ms.push_back((end - begin).count() / 1000);
    }

    // Bare log file scan.
    {
      std::vector<std::thread> threads(subscribers_count);
      const auto begin = current::time::Now();
      for (auto& t : threads) {
        t = std::thread([&]() {
          size_t lines = 0;
          {
            std::ifstream fi(FLAGS_file.c_str());
            std::string line;
            while (std::getline(fi, line)) {
              ++lines;
            }
          }
          CURRENT_ASSERT(lines == stream_size + 1);
        });
      }
      for (auto& t : threads) {
        t.join();
      }
      const auto end = current::time::Now();
      report.raw_file_scan_ms.push_back((end - begin).count() / 1000);
    }

    // Transaction-parsing log file scan.
    {
      std::vector<std::thread> threads(subscribers_count);
      const auto begin = current::time::Now();
      for (auto& t : threads) {
        t = std::thread([&]() {
          size_t lines = 0;
          {
            std::ifstream fi(FLAGS_file.c_str());
            std::string line;
            std::vector<std::string> pieces;
            bool first = true;
            while (std::getline(fi, line)) {
              if (first) {
                // Skip the first line with stream signature.
                CURRENT_ASSERT(line[0] == '#');
                first = false;
              } else {
                pieces = current::strings::Split(line, '\t');
                CURRENT_ASSERT(pieces.size() == 2u);
                CURRENT_ASSERT(ParseJSON<idxts_t>(pieces[0]).us.count() > 0);
                CURRENT_ASSERT(ParseJSON<transaction_t>(pieces[1]).mutations.size() == 1u);
                ++lines;
              }
            }
          }
          CURRENT_ASSERT(lines == stream_size);
        });
      }
      for (auto& t : threads) {
        t.join();
      }
      const auto end = current::time::Now();
      report.parsing_file_scan_ms.push_back((end - begin).count() / 1000);
    }
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  if (FLAGS_gen) {
    GenerateTestData(FLAGS_file, FLAGS_gen);
  } else {
    Report report;
    if (FLAGS_subs) {
      PerformReplayBenchmark(FLAGS_file, std::chrono::milliseconds(FLAGS_sleep_ms), FLAGS_subs, report);
    } else {
      for (uint16_t subs = 0; subs <= FLAGS_subs_range; ++subs) {
        PerformReplayBenchmark(FLAGS_file, std::chrono::milliseconds(FLAGS_sleep_ms), subs, report);
      }
      if (!FLAGS_json.empty()) {
        current::FileSystem::WriteStringToFile(JSON(report), FLAGS_json.c_str());
      }
      if (!FLAGS_png.empty()) {
        using namespace current::gnuplot;
        const std::string png = GNUPlot()
                                    .Title("Subscribers benchmark")
                                    .XLabel("Subscribers")
                                    .YLabel("Seconds")
                                    .ImageSize(1000)
                                    .OutputFormat("pngcairo")
                                    .Plot(WithMeta([&report](Plotter p) {
                                            for (size_t i = 0; i < report.subs.size(); ++i) {
                                              p(report.subs[i], 1e-3 * report.owning_storage_replay_ms[i]);
                                            }
                                          })
                                              .LineWidth(5)
                                              .Color("rgb '#B90000'")
                                              .Name("Owning storage"))
                                    .Plot(WithMeta([&report](Plotter p) {
                                            for (size_t i = 0; i < report.subs.size(); ++i) {
                                              p(report.subs[i], 1e-3 * report.following_storage_replay_ms[i]);
                                            }
                                          })
                                              .LineWidth(5)
                                              .Color("rgb '#0000B9'")
                                              .Name("Following storage"))
                                    .Plot(WithMeta([&report](Plotter p) {
                                            for (size_t i = 0; i < report.subs.size(); ++i) {
                                              p(report.subs[i], 1e-3 * report.raw_persister_replay_ms[i]);
                                            }
                                          })
                                              .LineWidth(5)
                                              .Color("rgb '#008080'")
                                              .Name("Persister iterators"))
                                    .Plot(WithMeta([&report](Plotter p) {
                                            for (size_t i = 0; i < report.subs.size(); ++i) {
                                              p(report.subs[i], 1e-3 * report.raw_file_scan_ms[i]);
                                            }
                                          })
                                              .LineWidth(5)
                                              .Color("rgb '#404040'")
                                              .Name("File scan w/o parsing"))
                                    .Plot(WithMeta([&report](Plotter p) {
                                            for (size_t i = 0; i < report.subs.size(); ++i) {
                                              p(report.subs[i], 1e-3 * report.parsing_file_scan_ms[i]);
                                            }
                                          })
                                              .LineWidth(5)
                                              .Color("rgb '#00B900'")
                                              .Name("File scan with parsing"));
        current::FileSystem::WriteStringToFile(png, FLAGS_png.c_str());
      }
    }
  }
}
