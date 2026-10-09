#pragma once

#include <funkyactors/actor.hpp>
#include <funkyactors/subscription.hpp>
#include <funkyactors/topic.hpp>
#include <memory>
#include <vector>

#include "snake/control_messages.hpp"
#include "snake/engine_manager_messages.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using funkyactors::Actor;
using funkyactors::SubscriptionPtr;
using funkyactors::TopicPtr;

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

  MockDirectionMsgSubscriber(ActorContext ctx, TopicPtr<DirectionMsg> topic)
      : Actor{ctx}, direction_sub_{create_sub(topic)} {}

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

  MockRenderableStateSubscriber(ActorContext ctx, TopicPtr<RenderableStateMsg> topic)
      : Actor{ctx}, state_sub_{create_sub(topic)} {}

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

  MockGameOverSubscriber(ActorContext ctx, TopicPtr<GameOverMsg> topic)
      : Actor{ctx}, gameover_sub_{create_sub(topic)} {}

 private:
  SubscriptionPtr<GameOverMsg> gameover_sub_;
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

  MockClockCommandSubscriber(ActorContext ctx, TopicPtr<GameClockCommandMsg> topic)
      : Actor{ctx}, clock_sub_{create_sub(topic)} {}

 private:
  SubscriptionPtr<GameClockCommandMsg> clock_sub_;
};

}  // namespace snake
