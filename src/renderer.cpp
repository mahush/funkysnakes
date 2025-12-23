#include "snake/renderer.hpp"

#include <iostream>
#include <vector>

namespace snake {

Renderer::Renderer(asio::io_context& io,
                   TopicPtr<StateUpdate> state_topic,
                   TopicPtr<GameOver> gameover_topic,
                   TopicPtr<LevelChange> level_topic)
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

  // Clear screen (simple version - just add newlines)
  std::cout << "\n\n";

  std::cout << "========== SNAKE GAME ==========\n";
  std::cout << "Game: " << state.game_id << " | Level: " << state.level << "\n";

  // Create board
  std::vector<std::vector<char>> board(state.board_height, std::vector<char>(state.board_width, '.'));

  // Draw snakes
  for (const auto& snake : state.snakes) {
    if (snake.alive && !snake.body.empty()) {
      // Draw head
      Point head = snake.body[0];
      if (head.y >= 0 && head.y < state.board_height && head.x >= 0 && head.x < state.board_width) {
        board[head.y][head.x] = (snake.player_id == "player1") ? 'A' : 'B';
      }

      // Draw body
      for (size_t i = 1; i < snake.body.size(); ++i) {
        Point segment = snake.body[i];
        if (segment.y >= 0 && segment.y < state.board_height && segment.x >= 0 && segment.x < state.board_width) {
          board[segment.y][segment.x] = (snake.player_id == "player1") ? 'a' : 'b';
        }
      }
    }
  }

  // Print board
  for (int y = 0; y < state.board_height; ++y) {
    for (int x = 0; x < state.board_width; ++x) {
      std::cout << board[y][x];
    }
    std::cout << "\n";
  }

  // Print player info
  std::cout << "================================\n";
  for (const auto& snake : state.snakes) {
    std::cout << snake.player_id << ": " << (snake.alive ? "ALIVE" : "DEAD")
              << " | Score: " << snake.score
              << " | Length: " << snake.body.size();
    if (!snake.body.empty()) {
      std::cout << " | Head: (" << snake.body[0].x << "," << snake.body[0].y << ")";
    }
    std::cout << "\n";
  }
  std::cout << "================================\n";
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
