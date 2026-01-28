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

// Timer type definitions for GameManager
struct RepositionTimerTag {};
using RepositionTimerElapsedEvent = TimerElapsedEvent<RepositionTimerTag>;
using RepositionTimerCommand = TimerCommand<RepositionTimerTag>;
using RepositionTimer = Timer<RepositionTimerElapsedEvent, RepositionTimerCommand>;
using RepositionTimerPtr = std::shared_ptr<RepositionTimer>;

struct LevelTimerTag {};
using LevelTimerElapsedEvent = TimerElapsedEvent<LevelTimerTag>;
using LevelTimerCommand = TimerCommand<LevelTimerTag>;
using LevelTimer = Timer<LevelTimerElapsedEvent, LevelTimerCommand>;
using LevelTimerPtr = std::shared_ptr<LevelTimer>;

/**
 * @brief Coordinates game lifecycle and sessions
 *
 * GameManager supervises game sessions and handles high-level
 * game control (start, stop).
 * Sends game clock control commands to GameEngine.
 */
class GameManager : public Actor<GameManager> {
 public:
  /**
   * @brief Construct a new Game Manager
   * @param ctx Actor execution context
   * @param gameover_topic Topic to subscribe for game over notifications
   * @param clock_topic Topic to publish game clock control commands
   * @param startgame_topic Topic to subscribe for start game commands
   * @param reposition_topic Topic to publish food reposition triggers
   * @param level_topic Topic to publish level changes
   * @param tickrate_topic Topic to publish tick rate changes
   * @param timer_factory Factory for creating timers
   */
  GameManager(Actor<GameManager>::ActorContext ctx, TopicPtr<GameOver> gameover_topic,
              TopicPtr<GameClockCommand> clock_topic, TopicPtr<StartGame> startgame_topic,
              TopicPtr<FoodRepositionTrigger> reposition_topic, TopicPtr<LevelChange> level_topic,
              TopicPtr<TickRateChange> tickrate_topic, TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processMessages() override;

 private:
  void onStartGame(const StartGame& msg);
  void onGameOver(const GameOver& msg);
  void onRepositionTimer();
  void onLevelTimer();

  // Publishers for sending messages
  PublisherPtr<GameClockCommand> clock_pub_;
  PublisherPtr<FoodRepositionTrigger> reposition_pub_;
  PublisherPtr<LevelChange> level_pub_;
  PublisherPtr<TickRateChange> tickrate_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<GameOver> gameover_sub_;
  SubscriptionPtr<StartGame> startgame_sub_;

  // Timers
  RepositionTimerPtr reposition_timer_;
  LevelTimerPtr level_timer_;

  GameId current_game_id_;
  int current_level_{1};
};

}  // namespace snake
