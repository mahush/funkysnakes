#pragma once

#include <asio.hpp>
#include <memory>

#include "actor-core/actor.hpp"
#include "actor-core/subscription.hpp"
#include "actor-core/timer/timer.hpp"
#include "actor-core/topic.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Actor;
using actor_core::make_cancel_command;
using actor_core::make_periodic_command;
using actor_core::PublisherPtr;
using actor_core::SubscriptionPtr;
using actor_core::Timer;
using actor_core::TimerCommand;
using actor_core::TimerElapsedEvent;
using actor_core::TimerFactoryPtr;
using actor_core::TopicPtr;

// Timer type definitions for GameEngineActor
struct GameTimerTag {};
using GameTimerElapsedEvent = TimerElapsedEvent<GameTimerTag>;
using GameTimerCommand = TimerCommand<GameTimerTag>;
using GameTimer = Timer<GameTimerElapsedEvent, GameTimerCommand>;
using GameTimerPtr = std::shared_ptr<GameTimer>;

/**
 * @brief Core game engine - manages game state and logic
 *
 * GameEngineActor holds all game state: board, snakes, food, scores, level.
 * Processes ticks, direction changes, and game logic.
 * Sends state updates and clock control messages via topics.
 */
class GameEngineActor : public Actor<GameEngineActor> {
 public:
  /**
   * @brief Construct a new Game Engine
   * @param ctx Actor execution context
   * @param direction_topic Topic to subscribe for direction changes
   * @param state_topic Topic to publish renderable state
   * @param clock_topic Topic to subscribe for clock control commands
   * @param tickrate_topic Topic to subscribe for tick rate changes
   * @param reposition_topic Topic to subscribe for food reposition triggers
   * @param alivests_topic Topic to publish player alive states
   * @param summary_req_topic Topic to subscribe for game state summary requests
   * @param summary_resp_topic Topic to publish game state summary responses
   * @param timer_factory Factory for creating timers
   */
  GameEngineActor(Actor<GameEngineActor>::ActorContext ctx, TopicPtr<DirectionChange> direction_topic,
                  TopicPtr<RenderableState> state_topic, TopicPtr<GameClockCommand> clock_topic,
                  TopicPtr<TickRateChange> tickrate_topic, TopicPtr<FoodRepositionTrigger> reposition_topic,
                  TopicPtr<PlayerAliveStates> alivests_topic, TopicPtr<GameStateSummaryRequest> summary_req_topic,
                  TopicPtr<GameStateSummaryResponse> summary_resp_topic, TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processInputs() override;

 private:
  // Publishers for sending messages
  PublisherPtr<RenderableState> renderable_state_pub_;
  PublisherPtr<PlayerAliveStates> alive_states_pub_;
  PublisherPtr<GameStateSummaryResponse> summary_resp_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<DirectionChange> direction_sub_;
  SubscriptionPtr<GameClockCommand> clock_sub_;
  SubscriptionPtr<TickRateChange> tickrate_sub_;
  SubscriptionPtr<FoodRepositionTrigger> reposition_sub_;
  SubscriptionPtr<GameStateSummaryRequest> summary_req_sub_;

  GameState game_state_;

  // Timer
  GameTimerPtr timer_;
};

}  // namespace snake
