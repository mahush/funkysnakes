#ifndef ACTOR_CORE_TOPIC_PUBLISHER_HPP
#define ACTOR_CORE_TOPIC_PUBLISHER_HPP

#include <memory>

namespace actor_core {

template<typename Msg>
class Topic;

template<typename Msg>
using TopicPtr = std::shared_ptr<Topic<Msg>>;

// Publisher interface for sending messages to a topic
// Provides symmetric API to Subscription
template<typename Msg>
class Publisher {
 public:
  explicit Publisher(TopicPtr<Msg> topic) : topic_(topic) {}

  // Publish a message to the topic
  // Takes by value to support both copy (lvalue) and move (rvalue) semantics
  void publish(Msg msg) {
    topic_->publish(std::move(msg));
  }

 private:
  TopicPtr<Msg> topic_;
};

// Convenience type alias for shared_ptr<Publisher<Msg>>
template<typename Msg>
using PublisherPtr = std::shared_ptr<Publisher<Msg>>;

}  // namespace actor_core

#endif  // ACTOR_CORE_TOPIC_PUBLISHER_HPP
