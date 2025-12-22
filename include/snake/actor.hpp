#ifndef SNAKE_ACTOR_HPP
#define SNAKE_ACTOR_HPP

#include "snake/message_processor_interface.hpp"
#include "snake/topic.hpp"
#include "snake/topic_publisher.hpp"
#include "snake/topic_subscription.hpp"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "asio.hpp"

namespace snake {

// CRTP base class for all actors
// Provides factory pattern, strand management, and subscription lifecycle
template<typename Derived>
class Actor : public MessageProcessorInterface,
              public std::enable_shared_from_this<Derived> {
 public:
  // Factory method - creates actor and finalizes subscriptions
  template<typename... Args>
  static std::shared_ptr<Derived> create(asio::io_context& io, Args&&... args) {
    // Use new instead of make_shared to allow protected constructor
    auto actor = std::shared_ptr<Derived>(new Derived(io, std::forward<Args>(args)...));

    // Finalize deferred subscriptions (now shared_from_this() works)
    actor->finalize();

    return actor;
  }

 protected:
  explicit Actor(asio::io_context& io) : strand_(asio::make_strand(io)) {}
  virtual ~Actor() = default;

  // Helper for derived classes to create subscriptions
  // Returns shared_ptr that can be initialized in initializer list
  // Defers actual registration until finalize() is called by factory
  template<typename Msg>
  std::shared_ptr<TopicSubscription<Msg>> create_sub(std::shared_ptr<Topic<Msg>> topic) {
    auto sub = std::make_shared<TopicSubscription<Msg>>();

    // Defer registration - will be completed in finalize()
    deferred_.push_back([this, topic, sub]() {
      // Register subscription with topic (now shared_from_this() works)
      // Topic holds weak_ptr to actor - auto-cleanup on actor destruction
      topic->subscribe(this->shared_from_this(), sub.get(), strand_);
    });

    return sub;
  }

  // Helper for derived classes to create publishers
  // Returns shared_ptr for symmetric API with create_sub
  template<typename Msg>
  std::shared_ptr<TopicPublisher<Msg>> create_pub(std::shared_ptr<Topic<Msg>> topic) {
    return std::make_shared<TopicPublisher<Msg>>(topic);
  }

  // Helper for async callbacks - use weak_ptr to avoid keeping actor alive
  std::weak_ptr<Derived> weak_from_this() {
    return this->shared_from_this();
  }

  asio::strand<asio::io_context::executor_type> strand_;

 private:
  // Finalize deferred subscriptions (called by factory after construction)
  void finalize() {
    for (auto& fn : deferred_) {
      fn();
    }
    deferred_.clear();
  }

  std::vector<std::function<void()>> deferred_;  // Deferred subscription registrations
};

}  // namespace snake

#endif  // SNAKE_ACTOR_HPP
