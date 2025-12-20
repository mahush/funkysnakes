#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/clock_sink.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Forward declaration
class GameSessionSink;

/**
 * @brief Periodically drives the game loop
 *
 * Clock sends Tick messages at configurable intervals.
 * Adjustable tick rate based on game level/difficulty.
 */
class Clock : public ClockSink, public std::enable_shared_from_this<Clock> {
 public:
  /**
   * @brief Construct a new Clock
   * @param io The io_context for async operations
   * @param session The game session that receives ticks
   */
  Clock(asio::io_context& io, std::shared_ptr<GameSessionSink> session);

  void post(StartClock msg) override;
  void post(StopClock msg) override;
  void post(TickRateChange msg) override;

 private:
  void onStartClock(const StartClock& msg);
  void onStopClock(const StopClock& msg);
  void onTickRateChange(const TickRateChange& msg);

  void scheduleTick();
  void onTimerExpired(const asio::error_code& ec);

  asio::strand<asio::io_context::executor_type> strand_;
  std::shared_ptr<GameSessionSink> session_;
  asio::steady_timer timer_;

  GameId current_game_id_;
  int interval_ms_{500};
  bool running_{false};
};

}  // namespace snake
