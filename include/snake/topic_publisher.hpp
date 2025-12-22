#ifndef SNAKE_TOPIC_PUBLISHER_HPP
#define SNAKE_TOPIC_PUBLISHER_HPP

#include <memory>

namespace snake {

template<typename Msg>
class Topic;

// Publisher interface for sending messages to a topic
// Provides symmetric API to TopicSubscription
template<typename Msg>
class TopicPublisher {
 public:
  explicit TopicPublisher(std::shared_ptr<Topic<Msg>> topic) : topic_(topic) {}

  // Publish a message to the topic
  // Takes by value to support both copy (lvalue) and move (rvalue) semantics
  void publish(Msg msg) {
    topic_->publish(std::move(msg));
  }

 private:
  std::shared_ptr<Topic<Msg>> topic_;
};

}  // namespace snake

#endif  // SNAKE_TOPIC_PUBLISHER_HPP
