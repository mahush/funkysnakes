#pragma once

#include <asio.hpp>
#include <memory>
#include <vector>

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

/**
 * @brief Coordinates game lifecycle, sessions, and player registry
 *
 * GameManager supervises all game sessions and handles high-level
 * game control (start, stop, player join/leave).
 * Sends game clock control commands to GameEngine.
 */
class GameManager : public Actor<GameManager> {
 public:
  // Process messages from subscribed topics
  void processMessages() override;

 protected:
  friend class Actor<GameManager>;

  /**
   * @brief Construct a new Game Manager
   * @param io The io_context for async operations
   * @param gameover_topic Topic to subscribe for game over notifications
   * @param clock_topic Topic to publish game clock control commands
   * @param joinrequest_topic Topic to subscribe for join requests
   * @param leaverequest_topic Topic to subscribe for leave requests
   * @param startgame_topic Topic to subscribe for start game commands
   * @param reposition_topic Topic to publish food reposition triggers
   * @param timer_factory Factory for creating timers
   */
  GameManager(asio::io_context& io,
              TopicPtr<GameOver> gameover_topic,
              TopicPtr<GameClockCommand> clock_topic,
              TopicPtr<JoinRequest> joinrequest_topic,
              TopicPtr<LeaveRequest> leaverequest_topic,
              TopicPtr<StartGame> startgame_topic,
              TopicPtr<FoodRepositionTrigger> reposition_topic,
              TimerFactoryPtr timer_factory);

 private:
  void onJoinRequest(const JoinRequest& msg);
  void onLeaveRequest(const LeaveRequest& msg);
  void onStartGame(const StartGame& msg);
  void onGameOver(const GameOver& msg);
  void onRepositionTimer();

  std::vector<PlayerId> registered_players_;

  // Publishers for sending messages
  PublisherPtr<GameClockCommand> clock_pub_;
  PublisherPtr<FoodRepositionTrigger> reposition_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<GameOver> gameover_sub_;
  SubscriptionPtr<JoinRequest> joinrequest_sub_;
  SubscriptionPtr<LeaveRequest> leaverequest_sub_;
  SubscriptionPtr<StartGame> startgame_sub_;

  // Timer for food repositioning
  RepositionTimerPtr timer_;

  GameId current_game_id_;
};

}  // namespace snake
