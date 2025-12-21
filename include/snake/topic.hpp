#ifndef SNAKE_TOPIC_HPP
#define SNAKE_TOPIC_HPP

#include "snake/message_sink.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace snake {

// Topic implements a typed pub-sub channel for a specific message type
// Uses weak_ptr to avoid circular references with subscribers
template<typename Msg>
class Topic {
 public:
  Topic() = default;

  // Subscribe to this topic
  // Topic holds weak_ptr to avoid keeping subscribers alive
  void subscribe(std::shared_ptr<MessageSink<Msg>> subscriber) {
    subscribers_.push_back(subscriber);
  }

  // Publish a message to all live subscribers
  // Automatically cleans up dead subscribers during publish
  void publish(Msg msg) {
    auto it = subscribers_.begin();
    while (it != subscribers_.end()) {
      if (auto sub = it->lock()) {
        // Subscriber still alive, deliver message
        sub->post(msg);
        ++it;
      } else {
        // Subscriber is dead, remove from list
        it = subscribers_.erase(it);
      }
    }
  }

 private:
  std::vector<std::weak_ptr<MessageSink<Msg>>> subscribers_;
};

}  // namespace snake

#endif  // SNAKE_TOPIC_HPP
