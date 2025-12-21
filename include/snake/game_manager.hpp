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
  void post(JoinRequest msg) override;
  void post(LeaveRequest msg) override;
  void post(StartGame msg) override;
  void post(GameOver msg) override;
  void post(StartClock msg) override;
  void post(StopClock msg) override;
  void post(TickRateChange msg) override;

 protected:
  friend class Actor<GameManager>;

  /**
   * @brief Construct a new Game Manager
   * @param io The io_context for async operations
   * @param tick_topic Topic to publish game ticks
   * @param gameover_topic Topic to subscribe for game over notifications
   * @param tickrate_topic Topic to subscribe for tick rate changes
   */
  GameManager(asio::io_context& io,
              std::shared_ptr<Topic<Tick>> tick_topic,
              std::shared_ptr<Topic<GameOver>> gameover_topic,
              std::shared_ptr<Topic<StartClock>> startclock_topic,
              std::shared_ptr<Topic<StopClock>> stopclock_topic,
              std::shared_ptr<Topic<TickRateChange>> tickrate_topic);

  void subscribeToTopics() override;

 private:
  void onJoinRequest(const JoinRequest& msg);
  void onLeaveRequest(const LeaveRequest& msg);
  void onStartGame(const StartGame& msg);
  void onGameOver(const GameOver& msg);

  void onStartClock(const StartClock& msg);
  void onStopClock(const StopClock& msg);
  void onTickRateChange(const TickRateChange& msg);

  void scheduleTick();
  void onTimerExpired(const asio::error_code& ec);

  asio::strand<asio::io_context::executor_type> strand_;
  std::vector<PlayerId> registered_players_;

  // Topics for publishing
  std::shared_ptr<Topic<Tick>> tick_topic_;

  // Topics for subscribing
  std::shared_ptr<Topic<GameOver>> gameover_topic_;
  std::shared_ptr<Topic<StartClock>> startclock_topic_;
  std::shared_ptr<Topic<StopClock>> stopclock_topic_;
  std::shared_ptr<Topic<TickRateChange>> tickrate_topic_;

  // Timer for game ticks
  asio::steady_timer timer_;
  GameId current_game_id_;
  int interval_ms_{500};
  bool running_{false};
};

}  // namespace snake
