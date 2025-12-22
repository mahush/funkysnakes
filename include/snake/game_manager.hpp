#pragma once

#include <asio.hpp>
#include <memory>
#include <vector>

#include "snake/actor.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/topic.hpp"
#include "snake/topic_subscription.hpp"

namespace snake {

/**
 * @brief Coordinates game lifecycle, sessions, and player registry
 *
 * GameManager supervises all game sessions and handles high-level
 * game control (start, stop, player join/leave).
 * Also owns the game timer and generates ticks.
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
   * @param tick_topic Topic to publish game ticks
   * @param gameover_topic Topic to subscribe for game over notifications
   * @param startclock_topic Topic to subscribe for clock start commands
   * @param stopclock_topic Topic to subscribe for clock stop commands
   * @param tickrate_topic Topic to subscribe for tick rate changes
   * @param joinrequest_topic Topic to subscribe for join requests
   * @param leaverequest_topic Topic to subscribe for leave requests
   * @param startgame_topic Topic to subscribe for start game commands
   */
  GameManager(asio::io_context& io,
              std::shared_ptr<Topic<Tick>> tick_topic,
              std::shared_ptr<Topic<GameOver>> gameover_topic,
              std::shared_ptr<Topic<StartClock>> startclock_topic,
              std::shared_ptr<Topic<StopClock>> stopclock_topic,
              std::shared_ptr<Topic<TickRateChange>> tickrate_topic,
              std::shared_ptr<Topic<JoinRequest>> joinrequest_topic,
              std::shared_ptr<Topic<LeaveRequest>> leaverequest_topic,
              std::shared_ptr<Topic<StartGame>> startgame_topic);

 private:
  void scheduleTick();
  void onTimerExpired(const asio::error_code& ec);

  void onJoinRequest(const JoinRequest& msg);
  void onLeaveRequest(const LeaveRequest& msg);
  void onStartGame(const StartGame& msg);
  void onGameOver(const GameOver& msg);
  void onStartClock(const StartClock& msg);
  void onStopClock(const StopClock& msg);
  void onTickRateChange(const TickRateChange& msg);

  std::vector<PlayerId> registered_players_;

  // Publishers for sending messages
  std::shared_ptr<TopicPublisher<Tick>> tick_pub_;
  std::shared_ptr<TopicPublisher<StartClock>> startclock_pub_;

  // Subscriptions for pulling messages
  std::shared_ptr<TopicSubscription<GameOver>> gameover_sub_;
  std::shared_ptr<TopicSubscription<StartClock>> startclock_sub_;
  std::shared_ptr<TopicSubscription<StopClock>> stopclock_sub_;
  std::shared_ptr<TopicSubscription<TickRateChange>> tickrate_sub_;
  std::shared_ptr<TopicSubscription<JoinRequest>> joinrequest_sub_;
  std::shared_ptr<TopicSubscription<LeaveRequest>> leaverequest_sub_;
  std::shared_ptr<TopicSubscription<StartGame>> startgame_sub_;

  // Timer for game ticks
  asio::steady_timer timer_;
  GameId current_game_id_;
  int interval_ms_{500};
  bool running_{false};
};

}  // namespace snake
