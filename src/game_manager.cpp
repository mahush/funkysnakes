#include "snake/game_manager.hpp"

#include <iostream>

#include "snake/clock_sink.hpp"
#include "snake/game_session_sink.hpp"
#include "snake/renderer_sink.hpp"

namespace snake {

GameManager::GameManager(asio::io_context& io, std::shared_ptr<GameSessionSink> session,
                         std::shared_ptr<ClockSink> clock, std::shared_ptr<RendererSink> renderer)
    : strand_(asio::make_strand(io)), session_(std::move(session)), clock_(std::move(clock)),
      renderer_(std::move(renderer)) {}

void GameManager::post(JoinRequest msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onJoinRequest(msg); });
}

void GameManager::post(LeaveRequest msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onLeaveRequest(msg); });
}

void GameManager::post(StartGame msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onStartGame(msg); });
}

void GameManager::post(GameOver msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onGameOver(msg); });
}

void GameManager::onJoinRequest(const JoinRequest& msg) {
  std::cout << "[GameManager] Player '" << msg.player_id << "' joined\n";
  registered_players_.push_back(msg.player_id);
}

void GameManager::onLeaveRequest(const LeaveRequest& msg) {
  std::cout << "[GameManager] Player '" << msg.player_id << "' left\n";
  // Remove from registered players (simple implementation)
  auto it = std::find(registered_players_.begin(), registered_players_.end(), msg.player_id);
  if (it != registered_players_.end()) {
    registered_players_.erase(it);
  }
}

void GameManager::onStartGame(const StartGame& msg) {
  std::cout << "[GameManager] Starting game with level " << msg.starting_level << " and " << msg.players.size()
            << " players\n";

  // Create StartSession message for GameSession
  StartSession session_msg;
  session_msg.game_id = "game_001";  // Simple ID for now
  session_msg.starting_level = msg.starting_level;
  session_msg.players = msg.players;

  // TODO: Send StartSession to GameSession (not yet implemented in interface)
  // For now, just start the clock
  StartClock clock_msg;
  clock_msg.game_id = session_msg.game_id;
  clock_msg.interval_ms = 500;  // 500ms per tick initially

  clock_->post(clock_msg);
}

void GameManager::onGameOver(const GameOver& msg) {
  std::cout << "[GameManager] Game '" << msg.summary.game_id << "' ended at level " << msg.summary.final_level << "\n";
  std::cout << "[GameManager] Final scores:\n";
  for (const auto& [player_id, score] : msg.summary.final_scores) {
    std::cout << "[GameManager]   " << player_id << ": " << score << "\n";
  }

  // Stop the clock
  StopClock stop_msg;
  stop_msg.game_id = msg.summary.game_id;
  clock_->post(stop_msg);
}

}  // namespace snake
