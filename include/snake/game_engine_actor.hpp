#pragma once

#include <asio.hpp>
#include <funkyactors/actor.hpp>
#include <funkyactors/subscription.hpp>
#include <funkyactors/timer/timer.hpp>
#include <funkyactors/topic.hpp>
#include <memory>

#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using funkyactors::Actor;
using funkyactors::make_cancel_command;
using funkyactors::make_periodic_command;
using funkyactors::PublisherPtr;
using funkyactors::SubscriptionPtr;
using funkyactors::Timer;
using funkyactors::TimerCommand;
using funkyactors::TimerElapsedEvent;
using funkyactors::TimerFactoryPtr;
using funkyactors::TopicPtr;

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
  GameEngineActor(ActorContext ctx,
                  TopicPtr<DirectionMsg> direction_topic,
                  TopicPtr<RenderableStateMsg> state_topic,
                  TopicPtr<GameClockCommandMsg> clock_topic,
                  TopicPtr<TickRateChangeMsg> tickrate_topic,
                  TopicPtr<FoodRepositionTriggerMsg> reposition_topic,
                  TopicPtr<PlayerAliveStatesMsg> alivests_topic,
                  TopicPtr<GameStateSummaryRequestMsg> summary_req_topic,
                  TopicPtr<GameStateSummaryResponseMsg> summary_resp_topic,
                  TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processInputs() override;

 private:
  // Publishers for sending messages
  PublisherPtr<RenderableStateMsg> renderable_state_pub_;
  PublisherPtr<PlayerAliveStatesMsg> alive_states_pub_;
  PublisherPtr<GameStateSummaryResponseMsg> summary_resp_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<DirectionMsg> direction_sub_;
  SubscriptionPtr<GameClockCommandMsg> clock_sub_;
  SubscriptionPtr<TickRateChangeMsg> tickrate_sub_;
  SubscriptionPtr<FoodRepositionTriggerMsg> reposition_sub_;
  SubscriptionPtr<GameStateSummaryRequestMsg> summary_req_sub_;

  GameState game_state_;

  // Timer
  GameTimerPtr game_loop_timer_;
};

}  // namespace snake
