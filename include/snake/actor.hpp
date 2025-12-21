#ifndef SNAKE_ACTOR_HPP
#define SNAKE_ACTOR_HPP

#include <memory>
#include <tuple>
#include <utility>

#include "asio.hpp"

namespace snake {

// CRTP base class for all actors
// Provides factory pattern, strand management, and automatic topic subscription
template<typename Derived>
class Actor : public std::enable_shared_from_this<Derived> {
 public:
  // Factory method - creates actor and subscribes to topics automatically
  // This is the only way to construct actors
  template<typename... Args>
  static std::shared_ptr<Derived> create(asio::io_context& io, Args&&... args) {
    // Use new instead of make_shared to allow protected constructor
    auto actor = std::shared_ptr<Derived>(new Derived(io, std::forward<Args>(args)...));

    // Get topics from derived class and subscribe automatically
    auto topics = actor->subscribeToTopics();
    std::apply([&](auto&&... topic_ptrs) {
      (topic_ptrs->subscribe(actor, actor->strand_), ...);
    }, topics);

    return actor;
  }

 protected:
  explicit Actor(asio::io_context& io) : strand_(asio::make_strand(io)) {}
  virtual ~Actor() = default;

  // Derived classes must implement: auto subscribeToTopics()
  // Should return std::make_tuple(topic1, topic2, ...) or std::make_tuple() for no subscriptions
  // Not declared here - uses CRTP duck typing to call derived implementation

  // Helper for async callbacks - use weak_ptr to avoid keeping actor alive
  std::weak_ptr<Derived> weak_from_this() {
    return this->shared_from_this();
  }

  asio::strand<asio::io_context::executor_type> strand_;
};

}  // namespace snake

#endif  // SNAKE_ACTOR_HPP
