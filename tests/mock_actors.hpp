#pragma once

#include <memory>
#include <vector>

#include "actor-core/actor.hpp"
#include "actor-core/subscription.hpp"
#include "actor-core/topic.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Actor;
using actor_core::SubscriptionPtr;
using actor_core::TopicPtr;

/**
 * @brief Mock subscriber for TickMsg messages
 */
class MockTickSubscriber : public Actor<MockTickSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = tick_sub_->tryTakeMessage()) {
      ticks.push_back(*msg);
    }
  }

  std::vector<TickMsg> ticks;

  MockTickSubscriber(Actor<MockTickSubscriber>::ActorContext ctx, TopicPtr<TickMsg> topic)
      : Actor(ctx), tick_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<TickMsg> tick_sub_;
};

/**
 * @brief Mock subscriber for DirectionMsg messages
 */
class MockDirectionMsgSubscriber : public Actor<MockDirectionMsgSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = direction_sub_->tryTakeMessage()) {
      direction_changes.push_back(*msg);
    }
  }

  std::vector<DirectionMsg> direction_changes;

  MockDirectionMsgSubscriber(Actor<MockDirectionMsgSubscriber>::ActorContext ctx, TopicPtr<DirectionMsg> topic)
      : Actor(ctx), direction_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<DirectionMsg> direction_sub_;
};

/**
 * @brief Mock subscriber for RenderableStateMsg messages
 */
class MockRenderableStateSubscriber : public Actor<MockRenderableStateSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = state_sub_->tryTakeMessage()) {
      renderable_states.push_back(*msg);
    }
  }

  std::vector<RenderableStateMsg> renderable_states;

  MockRenderableStateSubscriber(Actor<MockRenderableStateSubscriber>::ActorContext ctx, TopicPtr<RenderableStateMsg> topic)
      : Actor(ctx), state_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<RenderableStateMsg> state_sub_;
};

/**
 * @brief Mock subscriber for GameOverMsg messages
 */
class MockGameOverSubscriber : public Actor<MockGameOverSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = gameover_sub_->tryTakeMessage()) {
      game_overs.push_back(*msg);
    }
  }

  std::vector<GameOverMsg> game_overs;

  MockGameOverSubscriber(Actor<MockGameOverSubscriber>::ActorContext ctx, TopicPtr<GameOverMsg> topic)
      : Actor(ctx), gameover_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<GameOverMsg> gameover_sub_;
};

/**
 * @brief Mock subscriber for StartClockMsg messages
 */
class MockStartClockSubscriber : public Actor<MockStartClockSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = startclock_sub_->tryTakeMessage()) {
      start_clocks.push_back(*msg);
    }
  }

  std::vector<StartClockMsg> start_clocks;

  MockStartClockSubscriber(Actor<MockStartClockSubscriber>::ActorContext ctx, TopicPtr<StartClockMsg> topic)
      : Actor(ctx), startclock_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<StartClockMsg> startclock_sub_;
};

/**
 * @brief Mock subscriber for StopClockMsg messages
 */
class MockStopClockSubscriber : public Actor<MockStopClockSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = stopclock_sub_->tryTakeMessage()) {
      stop_clocks.push_back(*msg);
    }
  }

  std::vector<StopClockMsg> stop_clocks;

  MockStopClockSubscriber(Actor<MockStopClockSubscriber>::ActorContext ctx, TopicPtr<StopClockMsg> topic)
      : Actor(ctx), stopclock_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<StopClockMsg> stopclock_sub_;
};

/**
 * @brief Mock subscriber for GameClockCommandMsg messages
 */
class MockClockCommandSubscriber : public Actor<MockClockCommandSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = clock_sub_->tryTakeMessage()) {
      clock_commands.push_back(*msg);
    }
  }

  std::vector<GameClockCommandMsg> clock_commands;

  MockClockCommandSubscriber(Actor<MockClockCommandSubscriber>::ActorContext ctx, TopicPtr<GameClockCommandMsg> topic)
      : Actor(ctx), clock_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<GameClockCommandMsg> clock_sub_;
};

}  // namespace snake
