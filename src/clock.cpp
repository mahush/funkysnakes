#include "snake/clock.hpp"

#include <iostream>

#include "snake/game_session_sink.hpp"

namespace snake {

Clock::Clock(asio::io_context& io, std::shared_ptr<GameSessionSink> session)
    : strand_(asio::make_strand(io)), session_(std::move(session)), timer_(io) {}

void Clock::post(StartClock msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onStartClock(msg); });
}

void Clock::post(StopClock msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onStopClock(msg); });
}

void Clock::post(TickRateChange msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onTickRateChange(msg); });
}

void Clock::onStartClock(const StartClock& msg) {
  std::cout << "[Clock] Starting clock for game '" << msg.game_id << "' with interval " << msg.interval_ms << "ms\n";
  current_game_id_ = msg.game_id;
  interval_ms_ = msg.interval_ms;
  running_ = true;
  scheduleTick();
}

void Clock::onStopClock(const StopClock& msg) {
  std::cout << "[Clock] Stopping clock for game '" << msg.game_id << "'\n";
  running_ = false;
  timer_.cancel();
}

void Clock::onTickRateChange(const TickRateChange& msg) {
  std::cout << "[Clock] Changing tick rate for game '" << msg.game_id << "' to " << msg.interval_ms << "ms\n";
  interval_ms_ = msg.interval_ms;
  // If running, the new interval will take effect on the next tick
}

void Clock::scheduleTick() {
  if (!running_) {
    return;
  }

  timer_.expires_after(std::chrono::milliseconds(interval_ms_));
  timer_.async_wait(asio::bind_executor(
      strand_, [self = shared_from_this()](const asio::error_code& ec) { self->onTimerExpired(ec); }));
}

void Clock::onTimerExpired(const asio::error_code& ec) {
  if (ec == asio::error::operation_aborted) {
    // Clock was stopped
    return;
  }

  if (ec) {
    std::cerr << "[Clock] Timer error: " << ec.message() << "\n";
    return;
  }

  // Send tick to game session
  Tick tick;
  tick.game_id = current_game_id_;
  session_->post(tick);
  std::cout << "[Clock] tick'\n";

  // Schedule next tick
  scheduleTick();
}

}  // namespace snake
