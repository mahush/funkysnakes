#pragma once

#include <memory>
#include <vector>

#include "actor-core/actor.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/subscription.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Actor;
using actor_core::SubscriptionPtr;
using actor_core::TopicPtr;

/**
 * @brief Mock subscriber for Tick messages
 */
class MockTickSubscriber : public Actor<MockTickSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = tick_sub_->tryTakeMessage()) {
      ticks.push_back(*msg);
    }
  }

  std::vector<Tick> ticks;

   MockTickSubscriber(Actor<MockTickSubscriber>::ActorContext ctx, TopicPtr<Tick> topic)
      : Actor(ctx), tick_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<Tick> tick_sub_;
};

/**
 * @brief Mock subscriber for DirectionChange messages
 */
class MockDirectionChangeSubscriber : public Actor<MockDirectionChangeSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = direction_sub_->tryTakeMessage()) {
      direction_changes.push_back(*msg);
    }
  }

  std::vector<DirectionChange> direction_changes;

   MockDirectionChangeSubscriber(Actor<MockDirectionChangeSubscriber>::ActorContext ctx,
                                TopicPtr<DirectionChange> topic)
      : Actor(ctx), direction_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<DirectionChange> direction_sub_;
};

/**
 * @brief Mock subscriber for RenderableState messages
 */
class MockRenderableStateSubscriber : public Actor<MockRenderableStateSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = state_sub_->tryTakeMessage()) {
      renderable_states.push_back(*msg);
    }
  }

  std::vector<RenderableState> renderable_states;

   MockRenderableStateSubscriber(Actor<MockRenderableStateSubscriber>::ActorContext ctx,
                                 TopicPtr<RenderableState> topic)
      : Actor(ctx), state_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<RenderableState> state_sub_;
};

/**
 * @brief Mock subscriber for GameOver messages
 */
class MockGameOverSubscriber : public Actor<MockGameOverSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = gameover_sub_->tryTakeMessage()) {
      game_overs.push_back(*msg);
    }
  }

  std::vector<GameOver> game_overs;

   MockGameOverSubscriber(Actor<MockGameOverSubscriber>::ActorContext ctx, TopicPtr<GameOver> topic)
      : Actor(ctx), gameover_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<GameOver> gameover_sub_;
};

/**
 * @brief Mock subscriber for StartClock messages
 */
class MockStartClockSubscriber : public Actor<MockStartClockSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = startclock_sub_->tryTakeMessage()) {
      start_clocks.push_back(*msg);
    }
  }

  std::vector<StartClock> start_clocks;

   MockStartClockSubscriber(Actor<MockStartClockSubscriber>::ActorContext ctx,
                           TopicPtr<StartClock> topic)
      : Actor(ctx), startclock_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<StartClock> startclock_sub_;
};

/**
 * @brief Mock subscriber for StopClock messages
 */
class MockStopClockSubscriber : public Actor<MockStopClockSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = stopclock_sub_->tryTakeMessage()) {
      stop_clocks.push_back(*msg);
    }
  }

  std::vector<StopClock> stop_clocks;

   MockStopClockSubscriber(Actor<MockStopClockSubscriber>::ActorContext ctx, TopicPtr<StopClock> topic)
      : Actor(ctx), stopclock_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<StopClock> stopclock_sub_;
};

/**
 * @brief Mock subscriber for GameClockCommand messages
 */
class MockClockCommandSubscriber : public Actor<MockClockCommandSubscriber> {
 public:
  void processInputs() override {
    while (auto msg = clock_sub_->tryTakeMessage()) {
      clock_commands.push_back(*msg);
    }
  }

  std::vector<GameClockCommand> clock_commands;

   MockClockCommandSubscriber(Actor<MockClockCommandSubscriber>::ActorContext ctx,
                             TopicPtr<GameClockCommand> topic)
      : Actor(ctx), clock_sub_(create_sub(topic)) {}

 private:
  SubscriptionPtr<GameClockCommand> clock_sub_;
};

}  // namespace snake
