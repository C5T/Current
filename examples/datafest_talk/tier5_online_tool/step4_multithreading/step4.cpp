// To run: make

#include "current.h"

#include <thread>

FUNCTION(rides, output) {
  const int shards = 4;

  std::vector<std::vector<int>> MM(shards, std::vector<int>(12));

  std::vector<std::unique_ptr<std::thread>> threads(shards);
  for (int shard = 0; shard < shards; ++shard) {
    threads[shard] = std::make_unique<std::thread>([shard, shards, &rides, &MM]() {
      const size_t n = rides.size();
      const size_t begin = shard * n / shards;
      const size_t end = (shard + 1) * n / shards;
      for (size_t i = begin; i < end; ++i) {
        ++MM[shard][rides[i].pickup.month - 1];
      }
    });
  }

  for (auto& t : threads) {
    t->join();
  }

  for (int m = 0; m < 12; ++m) {
    int result = 0;
    for (int i = 0; i < shards; ++i) {
      result += MM[i][m];
    }
    output << Printf("%d %02d/2016\n", result, m + 1);
  }
}
