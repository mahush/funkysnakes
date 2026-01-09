#pragma once

#include <asio.hpp>
#include <memory>

#include "actor-core/actor.hpp"
#include "actor-core/timer/timer.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/topic_subscription.hpp"
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

// Timer type definitions for GameEngine
struct GameTimerTag {};
using GameTimerElapsedEvent = TimerElapsedEvent<GameTimerTag>;
using GameTimerCommand = TimerCommand<GameTimerTag>;
using GameTimer = Timer<GameTimerElapsedEvent, GameTimerCommand>;
using GameTimerPtr = std::shared_ptr<GameTimer>;

/**
 * @brief Core game engine - manages game state and logic
 *
 * GameEngine holds all game state: board, snakes, food, scores, level.
 * Processes ticks, direction changes, and game logic.
 * Sends state updates and clock control messages via topics.
 */
class GameEngine : public Actor<GameEngine> {
 public:
  // Process messages from subscribed topics
  void processMessages() override;

 protected:
  friend class Actor<GameEngine>;

  /**
   * @brief Construct a new Game Engine
   * @param io The io_context for async operations
   * @param direction_topic Topic to subscribe for direction changes
   * @param state_topic Topic to publish state updates
   * @param clock_topic Topic to subscribe for clock control commands
   * @param tickrate_topic Topic to subscribe for tick rate changes
   * @param levelchange_topic Topic to subscribe for level changes
   * @param reposition_topic Topic to subscribe for food reposition triggers
   * @param timer_factory Factory for creating timers
   */
  GameEngine(asio::io_context& io, TopicPtr<DirectionChange> direction_topic, TopicPtr<StateUpdate> state_topic,
             TopicPtr<GameClockCommand> clock_topic, TopicPtr<TickRateChange> tickrate_topic,
             TopicPtr<LevelChange> levelchange_topic, TopicPtr<FoodRepositionTrigger> reposition_topic,
             TimerFactoryPtr timer_factory);

 private:
  void onTick();

  // Control message handlers
  void onGameClockCommand(const GameClockCommand& msg);
  void onTickRateChange(const TickRateChange& msg);
  void onLevelChange(const LevelChange& msg);

  // Publishers for sending messages
  PublisherPtr<StateUpdate> state_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<DirectionChange> direction_sub_;
  SubscriptionPtr<GameClockCommand> clock_sub_;
  SubscriptionPtr<TickRateChange> tickrate_sub_;
  SubscriptionPtr<LevelChange> levelchange_sub_;
  SubscriptionPtr<FoodRepositionTrigger> reposition_sub_;

  GameState state_;

  // Timer
  GameTimerPtr timer_;
  int interval_ms_{200};  // Default 200ms, used for tick rate changes
};

}  // namespace snake
