#ifndef SNAKE_TOPIC_HPP
#define SNAKE_TOPIC_HPP

#include "snake/message_sink.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "asio.hpp"

namespace snake {

// Topic implements a typed pub-sub channel for a specific message type
// Uses weak_ptr to avoid circular references with subscribers
// Handles async dispatch to each subscriber's strand
template<typename Msg>
class Topic {
 public:
  Topic() = default;

  // Subscribe to this topic with a strand for async dispatch
  // Topic holds weak_ptr to avoid keeping subscribers alive
  void subscribe(std::shared_ptr<MessageSink<Msg>> subscriber,
                 asio::strand<asio::io_context::executor_type> strand) {
    subscriptions_.push_back({subscriber, strand});
  }

  // Publish a message to all live subscribers
  // Posts to each subscriber's strand for thread-safe async dispatch
  // Automatically cleans up dead subscribers during publish
  void publish(Msg msg) {
    auto it = subscriptions_.begin();
    while (it != subscriptions_.end()) {
      if (auto sub = it->subscriber.lock()) {
        // Subscriber still alive, post to its strand
        asio::post(it->strand, [weak_sub = it->subscriber, msg] {
          if (auto s = weak_sub.lock()) {
            s->onEvent(msg);
          }
        });
        ++it;
      } else {
        // Subscriber is dead, remove from list
        it = subscriptions_.erase(it);
      }
    }
  }

 private:
  struct Subscription {
    std::weak_ptr<MessageSink<Msg>> subscriber;
    asio::strand<asio::io_context::executor_type> strand;
  };
  std::vector<Subscription> subscriptions_;
};

}  // namespace snake

#endif  // SNAKE_TOPIC_HPP
