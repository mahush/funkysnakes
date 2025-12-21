#ifndef SNAKE_ACTOR_HPP
#define SNAKE_ACTOR_HPP

#include <memory>
#include <utility>

namespace snake {

// CRTP base class for all actors
// Provides factory pattern and weak_ptr helper for async callbacks
template<typename Derived>
class Actor : public std::enable_shared_from_this<Derived> {
 public:
  // Factory method - ensures subscribeToTopics() is called
  // This is the only way to construct actors
  template<typename... Args>
  static std::shared_ptr<Derived> create(Args&&... args) {
    // Use new instead of make_shared to allow protected constructor
    auto actor = std::shared_ptr<Derived>(new Derived(std::forward<Args>(args)...));
    actor->subscribeToTopics();
    return actor;
  }

 protected:
  Actor() = default;
  virtual ~Actor() = default;

  // Derived classes must implement this to subscribe to their topics
  virtual void subscribeToTopics() = 0;

  // Helper for async callbacks - use weak_ptr to avoid keeping actor alive
  std::weak_ptr<Derived> weak_from_this() {
    return this->shared_from_this();
  }
};

}  // namespace snake

#endif  // SNAKE_ACTOR_HPP
