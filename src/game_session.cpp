#include "snake/game_session.hpp"

#include <iostream>

#include "snake/clock_sink.hpp"
#include "snake/game_manager_sink.hpp"
#include "snake/renderer_sink.hpp"

namespace snake {

GameSession::GameSession(asio::io_context& io, std::shared_ptr<RendererSink> renderer, std::shared_ptr<ClockSink> clock,
                         std::shared_ptr<GameManagerSink> manager)
    : strand_(asio::make_strand(io)), renderer_(std::move(renderer)), clock_(std::move(clock)),
      manager_(std::move(manager)) {
  state_.game_id = "game_001";
  state_.running = false;
}

void GameSession::post(Tick msg) { asio::post(strand_, [self = shared_from_this(), msg] { self->onTick(msg); }); }

void GameSession::post(DirectionChange msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onDirectionChange(msg); });
}

void GameSession::post(PauseGame msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onPauseGame(msg); });
}

void GameSession::post(ResumeGame msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onResumeGame(msg); });
}

void GameSession::onTick(const Tick& msg) {
  if (!state_.running) {
    return;
  }

  std::cout << "[GameSession] Tick for game '" << msg.game_id << "'\n";

  // TODO: Actual game logic (move snakes, check collisions, etc.)
  // For now, just send state update
  StateUpdate update;
  update.state = state_;
  renderer_->post(update);
}

void GameSession::onDirectionChange(const DirectionChange& msg) {
  std::cout << "[GameSession] Player '" << msg.player_id << "' changed direction to "
            << static_cast<int>(msg.new_direction) << "\n";

  // TODO: Update snake direction in state
  // For now, just acknowledge
}

void GameSession::onPauseGame(const PauseGame& msg) {
  std::cout << "[GameSession] Game '" << msg.game_id << "' paused\n";
  state_.running = false;

  StopClock stop;
  stop.game_id = msg.game_id;
  clock_->post(stop);
}

void GameSession::onResumeGame(const ResumeGame& msg) {
  std::cout << "[GameSession] Game '" << msg.game_id << "' resumed\n";
  state_.running = true;

  StartClock start;
  start.game_id = msg.game_id;
  start.interval_ms = 500;  // TODO: Use actual tick rate
  clock_->post(start);
}

}  // namespace snake
