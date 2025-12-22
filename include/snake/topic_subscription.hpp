#ifndef SNAKE_TOPIC_SUBSCRIPTION_HPP
#define SNAKE_TOPIC_SUBSCRIPTION_HPP

#include <deque>
#include <mutex>
#include <optional>

namespace snake {

// Represents an actor's subscription to a specific topic
// Holds a bounded queue of pending messages
// Owned by the actor as a value type
template<typename Msg>
class Subscription {
 public:
  Subscription() = default;
  explicit Subscription(size_t max_queue_size) : max_queue_size_(max_queue_size) {}

  // Non-copyable, movable
  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;
  Subscription(Subscription&&) = default;
  Subscription& operator=(Subscription&&) = default;

  // Try to receive the next message from queue (non-blocking)
  // Returns std::nullopt if queue is empty
  std::optional<Msg> tryReceive() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    Msg msg = queue_.front();
    queue_.pop_front();
    return msg;
  }

  // Peek at next message without removing it
  std::optional<Msg> peek() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    return queue_.front();
  }

  // Check if there are pending messages
  bool hasMessages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !queue_.empty();
  }

  // Get number of pending messages
  size_t pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  // Internal: Add message to queue (called by Topic::publish)
  // Returns true if this is the first message (was empty before)
  bool enqueue(Msg msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If queue is full, drop oldest message
    if (queue_.size() >= max_queue_size_) {
      queue_.pop_front();
    }

    bool was_empty = queue_.empty();
    queue_.push_back(std::move(msg));
    return was_empty;
  }

 private:
  mutable std::mutex mutex_;
  std::deque<Msg> queue_;
  size_t max_queue_size_{100};  // Default max queue size
};

// Convenience type alias for shared_ptr<Subscription<Msg>>
template<typename Msg>
using SubscriptionPtr = std::shared_ptr<Subscription<Msg>>;

}  // namespace snake

#endif  // SNAKE_TOPIC_SUBSCRIPTION_HPP
