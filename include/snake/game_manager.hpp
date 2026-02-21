#pragma once

#include <asio.hpp>
#include <memory>

#include "actor-core/actor.hpp"
#include "actor-core/timer/timer.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/subscription.hpp"
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
   * @param clock_topic Topic to publish game clock control commands
   * @param startgame_topic Topic to subscribe for start game commands
   * @param reposition_topic Topic to publish food reposition triggers
   * @param metadata_topic Topic to publish game state metadata (level, pause state)
   * @param tickrate_topic Topic to publish tick rate changes
   * @param alivests_topic Topic to subscribe for player alive states
   * @param summary_req_topic Topic to publish game state summary requests
   * @param summary_resp_topic Topic to subscribe for game state summary responses
   * @param gameover_topic Topic to publish game over notifications
   * @param pause_topic Topic to subscribe for pause toggle requests
   * @param timer_factory Factory for creating timers
   */
  GameManager(Actor<GameManager>::ActorContext ctx, TopicPtr<GameClockCommand> clock_topic,
              TopicPtr<StartGame> startgame_topic, TopicPtr<FoodRepositionTrigger> reposition_topic,
              TopicPtr<GameStateMetadata> metadata_topic, TopicPtr<TickRateChange> tickrate_topic,
              TopicPtr<PlayerAliveStates> alivests_topic, TopicPtr<GameStateSummaryRequest> summary_req_topic,
              TopicPtr<GameStateSummaryResponse> summary_resp_topic, TopicPtr<GameOver> gameover_topic,
              TopicPtr<PauseToggle> pause_topic, TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processInputs() override;

 private:
  void onStartGame(const StartGame& msg);
  void onPlayerAliveStates(const PlayerAliveStates& msg);
  void onSummaryResponse(const GameStateSummaryResponse& response);
  void onPauseToggle(const PauseToggle& msg);
  void onRepositionTimer();
  void onLevelTimer();
  void publishMetadata();

  // Publishers for sending messages
  PublisherPtr<GameClockCommand> clock_pub_;
  PublisherPtr<FoodRepositionTrigger> reposition_pub_;
  PublisherPtr<GameStateMetadata> metadata_pub_;
  PublisherPtr<TickRateChange> tickrate_pub_;
  PublisherPtr<GameStateSummaryRequest> summary_req_pub_;
  PublisherPtr<GameOver> gameover_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<StartGame> startgame_sub_;
  SubscriptionPtr<PlayerAliveStates> alive_states_sub_;
  SubscriptionPtr<GameStateSummaryResponse> summary_resp_sub_;
  SubscriptionPtr<PauseToggle> pause_sub_;

  // Timers
  RepositionTimerPtr reposition_timer_;
  LevelTimerPtr level_timer_;

  GameId current_game_id_;
  int current_level_{1};
  bool game_over_detected_{false};
  bool paused_{false};
};

}  // namespace snake
