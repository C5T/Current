/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#ifndef REMOTE_STREAM_REPLICATOR_H
#define REMOTE_STREAM_REPLICATOR_H

namespace current {
namespace storage {

template <typename T_STREAM>
class RemoteStreamReplicator {
 public:
  using publisher_t = typename T_STREAM::publisher_t;
  using entry_t = typename T_STREAM::entry_t;
  
  explicit RemoteStreamReplicator(const std::string& remote_stream_url) : remote_stream_url_(remote_stream_url), index_(0), stop_(false) {}

  ~RemoteStreamReplicator() {
    if (thread_) {
      TerminateSubscription();
      thread_->join();
    }
  }
  
  void Subscribe() {
    Subscribe(index_);
  }
  
  void Subscribe(uint64_t index) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!publisher_)
      CURRENT_THROW(PublisherDoesNotExistException());
    if (thread_)
      CURRENT_THROW(SubscriptionIsActiveException());
    index_ = index;
    thread_.reset(new std::thread([this](){ Thread(); }));
  }
  
  void Unsubscribe() {
    Unsubscribe(index_);
  }
  
  void Unsubscribe(uint64_t index) {
    if (!thread_)
      CURRENT_THROW(SubscriptionIsNotActiveException());
    Wait(index);
    TerminateSubscription();
    thread_->join();
    thread_.reset();
  }
  
  void Wait(uint64_t index) const {
    std::unique_lock<std::mutex> lock(mutex_);
    while (index_ < index) {
      event_.wait(lock);
    }
  }

  void AcceptPublisher(std::unique_ptr<publisher_t> publisher) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (publisher_)
      CURRENT_THROW(PublisherAlreadyOwnedException());
    publisher_ = std::move(publisher);
  }

  void ReturnPublisherToStream(T_STREAM& stream) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!publisher_)
      CURRENT_THROW(PublisherAlreadyReleasedException());
    if (thread_)
      CURRENT_THROW(PublisherIsUsedException());
    stream.AcquirePublisher(std::move(publisher_));
  }

 private:
  void TerminateSubscription()
  {
    stop_ = true;
    std::unique_lock<std::mutex> lock(mutex_);
    if (!subscription_id_.empty()) {
      const std::string terminate_url = remote_stream_url_ + "?terminate=" + subscription_id_;
      try {
        const auto result = HTTP(GET(terminate_url));
      } catch (current::net::NetworkException& e) {
      }
    }
  }

  void Thread()
  {
	  while (!stop_) {
		  const std::string url = remote_stream_url_ + "?i=" + current::ToString(index_);
		  try {
			  HTTP(
				   ChunkedGET(url,
							  [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
							  [this](const std::string& chunk_body) { OnChunk(chunk_body); },
							  [this]() {}));
		  } catch (current::net::NetworkException&) {
		  }
	  }
  }
	
  void OnHeader(const std::string& header, const std::string& value) {
		if (header == "X-Current-Stream-Subscription-Id") {
      std::unique_lock<std::mutex> lock(mutex_);
			subscription_id_ = value;
		}
	}

	void OnChunk(const std::string& chunk) {
		if (stop_) {
			return;
		}
		std::unique_lock<std::mutex> lock(mutex_);
    
    const auto split = current::strings::Split(chunk, '\t');
    assert(split.size() == 2u);
    const auto idx = ParseJSON<idxts_t>(split[0]);
    assert(idx.index == index_);
    auto transaction = ParseJSON<entry_t>(split[1]);
    publisher_->Publish(std::move(transaction), idx.us);
    ++index_;
    event_.notify_one();
	}
	
private:
  const std::string remote_stream_url_;
  uint64_t index_;
  std::string subscription_id_;
  std::unique_ptr<std::thread> thread_;
  mutable std::mutex mutex_;
  mutable std::condition_variable event_;
  std::atomic_bool stop_;
  std::unique_ptr<publisher_t> publisher_;
};


}  // namespace storage
}  // namespace current

#endif  // REMOTE_STREAM_REPLICATOR_H
