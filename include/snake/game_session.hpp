#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/actor.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/state_with_effect.hpp"
#include "snake/timer/timer.hpp"
#include "snake/topic.hpp"
#include "snake/topic_subscription.hpp"

namespace snake {

// Timer type definitions for GameSession
struct GameTimerTag {};
using GameTimerElapsedEvent = TimerElapsedEvent<GameTimerTag>;
using GameTimerCommand = TimerCommand<GameTimerTag>;
using GameTimer = Timer<GameTimerElapsedEvent, GameTimerCommand>;
using GameTimerPtr = std::shared_ptr<GameTimer>;

/**
 * @brief Collision handling mode
 *
 * Determines what happens when snakes collide.
 */
enum class CollisionMode {
  BITE_REMOVE_TAIL,  // Cut tail is simply removed
  BITE_DROP_FOOD     // Cut tail segments become food items
};

/**
 * @brief Core game engine - manages game state and logic
 *
 * GameSession holds all game state: board, snakes, food, scores, level.
 * Processes ticks, direction changes, and game logic.
 * Sends state updates and clock control messages via topics.
 */
class GameSession : public Actor<GameSession> {
 public:
  // Process messages from subscribed topics
  void processMessages() override;

 protected:
  friend class Actor<GameSession>;

  /**
   * @brief Construct a new Game Session
   * @param io The io_context for async operations
   * @param direction_topic Topic to subscribe for direction changes
   * @param state_topic Topic to publish state updates
   * @param clock_topic Topic to subscribe for clock control commands
   * @param tickrate_topic Topic to subscribe for tick rate changes
   * @param levelchange_topic Topic to subscribe for level changes
   * @param timer_factory Factory for creating timers
   */
  GameSession(asio::io_context& io,
              TopicPtr<DirectionChange> direction_topic,
              TopicPtr<StateUpdate> state_topic,
              TopicPtr<GameClockCommand> clock_topic,
              TopicPtr<TickRateChange> tickrate_topic,
              TopicPtr<LevelChange> levelchange_topic,
              TimerFactoryPtr timer_factory);

 private:
  void onTick();
  void onDirectionChange(const DirectionChange& msg);

  // Control message handlers
  void onGameClockCommand(const GameClockCommand& msg);
  void onTickRateChange(const TickRateChange& msg);
  void onLevelChange(const LevelChange& msg);

  // Game initialization helpers
  void initializeSnake(const PlayerId& player_id);
  void initializeFood();
  Point generateRandomFoodPosition() const;

  // Publishers for sending messages
  PublisherPtr<StateUpdate> state_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<DirectionChange> direction_sub_;
  SubscriptionPtr<GameClockCommand> clock_sub_;
  SubscriptionPtr<TickRateChange> tickrate_sub_;
  SubscriptionPtr<LevelChange> levelchange_sub_;

  GameState state_;
  GameEffect pending_effects_;  // Accumulate effects between ticks (e.g., direction changes)
  CollisionMode collision_mode_{CollisionMode::BITE_REMOVE_TAIL};
  int tick_count_{0};

  // Timer
  GameTimerPtr timer_;
  int interval_ms_{200};  // Default 200ms, used for tick rate changes
};

}  // namespace snake
