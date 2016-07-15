#include "../../../Bricks/dflags/dflags.h"

//#include "../../../Bricks/file/file.h"

#include "schema.h"

DEFINE_string(file, "", "Storage persistence file in Sherlock format to use.");
DEFINE_bool(gen, false, "Generate test data and save to the file specified by `--file.`");
DEFINE_uint32(size, 0u, "If `--gen` is set, the number of entries to generate.");
DEFINE_int16(sleep,
             10,
             "The time in milliseconds to sleep in the spin-lock while checking "
             "actual replayed entries count.");

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

inline void PerformReplayBenchmark(const std::string& file, std::chrono::milliseconds spin_lock_sleep) {
  using stream_t = typename storage_t::persister_t::sherlock_t;

  // Owning storage replay.
  {
    std::cout << "=== Owning storage ===" << std::endl;
    const auto begin = current::time::Now();
    stream_t stream(file);
    const auto stream_initialized = current::time::Now();
    storage_t storage(stream);
    const auto end = current::time::Now();
    std::cout << "* Stream initialization: " << (stream_initialized - begin).count() / 1000 << " ms\n";
    std::cout << "* Storage replay: " << (end - stream_initialized).count() / 1000 << " ms" << std::endl;
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
    std::cout << "* Storage replay: " << (end - stream_initialized).count() / 1000 << " ms" << std::endl;
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  if (FLAGS_gen) {
    GenerateTestData(FLAGS_file, FLAGS_size);
  } else {
    PerformReplayBenchmark(FLAGS_file, std::chrono::milliseconds(FLAGS_sleep));
  }
}
