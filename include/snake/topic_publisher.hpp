#ifndef SNAKE_TOPIC_PUBLISHER_HPP
#define SNAKE_TOPIC_PUBLISHER_HPP

#include <memory>

namespace snake {

template<typename Msg>
class Topic;

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

}  // namespace snake

#endif  // SNAKE_TOPIC_PUBLISHER_HPP
