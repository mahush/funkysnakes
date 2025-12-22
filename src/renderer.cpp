#include "snake/renderer.hpp"

#include <iostream>

namespace snake {

Renderer::Renderer(asio::io_context& io,
                   std::shared_ptr<Topic<StateUpdate>> state_topic,
                   std::shared_ptr<Topic<GameOver>> gameover_topic,
                   std::shared_ptr<Topic<LevelChange>> level_topic)
    : Actor(io),
      state_sub_(create_sub(state_topic)),
      gameover_sub_(create_sub(gameover_topic)),
      level_sub_(create_sub(level_topic)) {
}

void Renderer::processMessages() {
  // Process all pending state updates
  while (auto msg = state_sub_->tryReceive()) {
    onStateUpdate(*msg);
  }

  // Process game over messages
  while (auto msg = gameover_sub_->tryReceive()) {
    onGameOver(*msg);
  }

  // Process level changes
  while (auto msg = level_sub_->tryReceive()) {
    onLevelChange(*msg);
  }
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
