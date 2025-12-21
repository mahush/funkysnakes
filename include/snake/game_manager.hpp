#pragma once

#include <asio.hpp>
#include <memory>
#include <vector>

#include "snake/actor.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/message_sink.hpp"
#include "snake/topic.hpp"

namespace snake {

/**
 * @brief Coordinates game lifecycle, sessions, and player registry
 *
 * GameManager supervises all game sessions and handles high-level
 * game control (start, stop, player join/leave).
 * Also owns the game timer and generates ticks.
 */
class GameManager : public Actor<GameManager>,
                    public MessageSink<JoinRequest>,
                    public MessageSink<LeaveRequest>,
                    public MessageSink<StartGame>,
                    public MessageSink<GameOver>,
                    public MessageSink<StartClock>,
                    public MessageSink<StopClock>,
                    public MessageSink<TickRateChange> {
 public:
  // MessageSink interface implementations
  void onEvent(JoinRequest msg) override;
  void onEvent(LeaveRequest msg) override;
  void onEvent(StartGame msg) override;
  void onEvent(GameOver msg) override;
  void onEvent(StartClock msg) override;
  void onEvent(StopClock msg) override;
  void onEvent(TickRateChange msg) override;

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

  auto subscribeToTopics() {
    return std::make_tuple(gameover_topic_, startclock_topic_, stopclock_topic_, tickrate_topic_,
                           joinrequest_topic_, leaverequest_topic_, startgame_topic_);
  }

 private:
  void scheduleTick();
  void onTimerExpired(const asio::error_code& ec);

  std::vector<PlayerId> registered_players_;

  // Topics for publishing
  std::shared_ptr<Topic<Tick>> tick_topic_;

  // Topics for subscribing
  std::shared_ptr<Topic<GameOver>> gameover_topic_;
  std::shared_ptr<Topic<StartClock>> startclock_topic_;
  std::shared_ptr<Topic<StopClock>> stopclock_topic_;
  std::shared_ptr<Topic<TickRateChange>> tickrate_topic_;
  std::shared_ptr<Topic<JoinRequest>> joinrequest_topic_;
  std::shared_ptr<Topic<LeaveRequest>> leaverequest_topic_;
  std::shared_ptr<Topic<StartGame>> startgame_topic_;

  // Timer for game ticks
  asio::steady_timer timer_;
  GameId current_game_id_;
  int interval_ms_{500};
  bool running_{false};
};

}  // namespace snake
