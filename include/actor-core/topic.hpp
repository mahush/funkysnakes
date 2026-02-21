#ifndef ACTOR_CORE_TOPIC_HPP
#define ACTOR_CORE_TOPIC_HPP

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include "asio.hpp"
#include "actor-core/processor_interface.hpp"
#include "actor-core/subscription.hpp"

namespace actor_core {

// Forward declarations
template <typename Msg>
class Publisher;

template <typename Derived>
class Actor;

// Topic implements a typed pub-sub channel for a specific message type
// Uses pull-based model: actors pull messages from their TopicSubscription
// Thread-safe: multiple actors can publish concurrently
template <typename Msg>
class Topic {
 public:
  Topic() = default;

 private:
  // Grant access to Publisher for publish()
  friend class Publisher<Msg>;

  // Grant access to Actor for subscribe() in create_sub()
  template <typename Derived>
  friend class Actor;

  // Subscribe to this topic
  // processor: Actor that will be notified when messages arrive (via weak_ptr)
  // subscription: Actor's subscription queue (raw pointer - actor owns it)
  // strand: Strand to post notifications on
  void subscribe(std::weak_ptr<ProcessorInterface> processor, Subscription<Msg>* subscription,
                 asio::strand<asio::io_context::executor_type> strand) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.push_back({processor, subscription, strand});
  }

  // Unsubscribe a subscription (currently unused - cleanup is lazy via weak_ptr)
  void unsubscribe(Subscription<Msg>* subscription) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [subscription](const SubscriptionEntry& s) { return s.subscription == subscription; }),
        subscriptions_.end());
  }

  // Publish a message to all live subscribers
  // Enqueues message in each subscriber's queue
  // Notifies processor only if queue was empty (to avoid redundant notifications)
  // Thread-safe: multiple threads can call this concurrently
  void publish(Msg msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscriptions_.begin();
    while (it != subscriptions_.end()) {
      if (auto processor = it->processor.lock()) {
        // Processor still alive, enqueue message
        bool was_empty = it->subscription->enqueue(msg);

        // Only notify if queue transitioned from empty to non-empty
        // This avoids spamming notifications while messages are being processed
        if (was_empty) {
          asio::post(it->strand, [weak_proc = it->processor]() {
            if (auto p = weak_proc.lock()) {
              p->processMessages();
            }
          });
        }

        ++it;
      } else {
        // Processor is dead, remove from list
        it = subscriptions_.erase(it);
      }
    }
  }

  struct SubscriptionEntry {
    std::weak_ptr<ProcessorInterface> processor;
    Subscription<Msg>* subscription;  // Raw pointer - owned by actor
    asio::strand<asio::io_context::executor_type> strand;
  };

  std::mutex mutex_;  // Protects subscriptions_ from concurrent access
  std::vector<SubscriptionEntry> subscriptions_;
};

// Convenience type alias for shared_ptr<Topic<Msg>>
template<typename Msg>
using TopicPtr = std::shared_ptr<Topic<Msg>>;

}  // namespace actor_core

#endif  // ACTOR_CORE_TOPIC_HPP
