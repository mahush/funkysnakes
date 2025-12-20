#include "snake/renderer.hpp"

#include <iostream>

namespace snake {

Renderer::Renderer(asio::io_context& io) : strand_(asio::make_strand(io)) {}

void Renderer::post(StateUpdate msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onStateUpdate(msg); });
}

void Renderer::post(GameOver msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onGameOver(msg); });
}

void Renderer::post(LevelChange msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onLevelChange(msg); });
}

void Renderer::onStateUpdate(const StateUpdate& msg) {
  const auto& state = msg.state;
  std::cout << "[Renderer] ========== Game State ==========\n";
  std::cout << "[Renderer] Game: " << state.game_id << " | Level: " << state.level
            << " | Running: " << (state.running ? "Yes" : "No") << "\n";
  std::cout << "[Renderer] Board: " << state.board_width << "x" << state.board_height << "\n";
  std::cout << "[Renderer] Food at: (" << state.food.x << ", " << state.food.y << ")\n";

  for (const auto& snake : state.snakes) {
    std::cout << "[Renderer] Snake '" << snake.player_id << "': Score=" << snake.score
              << ", Alive=" << (snake.alive ? "Yes" : "No") << ", Length=" << snake.body.size() << "\n";
  }
  std::cout << "[Renderer] ================================\n";
}

void Renderer::onGameOver(const GameOver& msg) {
  std::cout << "[Renderer] ********** GAME OVER **********\n";
  std::cout << "[Renderer] Game: " << msg.summary.game_id << " | Final Level: " << msg.summary.final_level << "\n";
  std::cout << "[Renderer] Final Scores:\n";
  for (const auto& [player_id, score] : msg.summary.final_scores) {
    std::cout << "[Renderer]   " << player_id << ": " << score << "\n";
  }
  std::cout << "[Renderer] ******************************\n";
}

void Renderer::onLevelChange(const LevelChange& msg) {
  std::cout << "[Renderer] >>>>>> LEVEL UP! Now at level " << msg.new_level << " <<<<<<\n";
}

}  // namespace snake
