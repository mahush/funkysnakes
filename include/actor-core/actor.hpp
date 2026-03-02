#ifndef ACTOR_CORE_ACTOR_HPP
#define ACTOR_CORE_ACTOR_HPP

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "actor-core/processor_interface.hpp"
#include "actor-core/publisher.hpp"
#include "actor-core/subscription.hpp"
#include "actor-core/timer/timer_factory.hpp"
#include "actor-core/topic.hpp"
#include "asio.hpp"

namespace actor_core {

// CRTP base class for all actors
// Provides factory pattern, strand management, and subscription lifecycle
template <typename TDerived>
class Actor : public ProcessorInterface, public std::enable_shared_from_this<TDerived> {
 public:
  // Execution context wrapper - can only be constructed by factory
  // Abstracts away asio implementation details from actor constructors
  class ActorContext {
   public:
    asio::io_context& io_context() { return io_; }

   private:
    asio::io_context& io_;
    explicit ActorContext(asio::io_context& io) : io_(io) {}
    friend class Actor<TDerived>;  // Only Actor can construct
  };

  // Factory method - creates actor and finalizes subscriptions
  template <typename... Args>
  static std::shared_ptr<TDerived> create(asio::io_context& io, Args&&... args) {
    // Wrap io_context in ActorContext (only factory can do this)
    ActorContext ctx{io};
    auto actor = std::make_shared<TDerived>(ctx, std::forward<Args>(args)...);

    // Finalize deferred subscriptions (now shared_from_this() works)
    actor->finalize();

    return actor;
  }

 protected:
  explicit Actor(ActorContext ctx) : strand_(asio::make_strand(ctx.io_context())) {}
  virtual ~Actor() = default;

  // Helper for derived classes to create subscriptions
  // Returns shared_ptr that can be initialized in initializer list
  // Defers actual registration until finalize() is called by factory
  template <typename TMsg>
  SubscriptionPtr<TMsg> create_sub(TopicPtr<TMsg> topic) {
    auto sub = std::make_shared<Subscription<TMsg>>();

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
  template <typename TMsg>
  PublisherPtr<TMsg> create_pub(TopicPtr<TMsg> topic) {
    return std::make_shared<Publisher<TMsg>>(topic);
  }

  // Helper for derived classes to create timers
  // Creates timer immediately but defers subscription until finalize()
  template <typename TTimer>
  std::shared_ptr<TTimer> create_timer(TimerFactoryPtr factory) {
    auto timer = factory->create<TTimer>(strand_);

    // Defer subscription - will be completed in finalize()
    deferred_.push_back([this, timer]() {
      // Subscribe this actor to timer events (now shared_from_this() works)
      timer->subscribe(this->shared_from_this());
    });

    return timer;
  }

  // Helper for async callbacks - use weak_ptr to avoid keeping actor alive
  std::weak_ptr<TDerived> weak_from_this() { return this->shared_from_this(); }

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

}  // namespace actor_core

#endif  // ACTOR_CORE_ACTOR_HPP
