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

#ifndef CURRENT_SHERLOCK_STREAM_DATA_H
#define CURRENT_SHERLOCK_STREAM_DATA_H

#include "../port.h"

#include <map>
#include <thread>

#include "../Blocks/Persistence/persistence.h"
#include "../Bricks/util/random.h"
#include "../Bricks/util/sha256.h"
#include "../Bricks/util/waitable_terminate_signal.h"

namespace current {
namespace sherlock {

class AbstractSubscriptionThread {
 public:
  virtual ~AbstractSubscriptionThread() = default;
};

class AbstractSubscriberObject {
 public:
  virtual ~AbstractSubscriberObject() = default;
};

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER>
struct StreamData {
  using entry_t = ENTRY;
  using persistence_layer_t = PERSISTENCE_LAYER<entry_t>;

  persistence_layer_t persistence;
  current::WaitableTerminateSignalBulkNotifier notifier;
  std::mutex publish_mutex;

  std::unordered_map<std::string,
                     std::pair<std::unique_ptr<AbstractSubscriptionThread>,
                               std::unique_ptr<AbstractSubscriberObject>>> http_subscriptions;
  std::mutex http_subscriptions_mutex;

  template <typename... ARGS>
  StreamData(ARGS&&... args)
      : persistence(std::forward<ARGS>(args)...) {}

  static std::string GenerateRandomHTTPSubscriptionID() {
    return current::SHA256("sherlock_http_subsrciption_" +
                           current::ToString(current::random::CSRandomUInt64(0ull, ~0ull)));
  }
};

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_STREAM_DATA_H
