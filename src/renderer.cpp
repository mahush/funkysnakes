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
  std::vector<std::vector<char>> board(state.board.height, std::vector<char>(state.board.width, '.'));

  // Draw food items
  for (const Point& food : state.food_items) {
    if (food.y >= 0 && food.y < state.board.height && food.x >= 0 && food.x < state.board.width) {
      board[food.y][food.x] = '*';
    }
  }

  // Draw snakes (drawn after food so snakes appear on top)
  for (const auto& [player_id, snake] : state.snakes) {
    if (snake.alive) {
      // Draw head
      Point head = snake.head;
      if (head.y >= 0 && head.y < state.board.height && head.x >= 0 && head.x < state.board.width) {
        board[head.y][head.x] = (player_id == "player1") ? 'A' : 'B';
      }

      // Draw tail
      for (const Point& segment : snake.tail) {
        if (segment.y >= 0 && segment.y < state.board.height && segment.x >= 0 && segment.x < state.board.width) {
          board[segment.y][segment.x] = (player_id == "player1") ? 'a' : 'b';
        }
      }
    }
  }

  // Print board
  for (int y = 0; y < state.board.height; ++y) {
    for (int x = 0; x < state.board.width; ++x) {
      std::cout << board[y][x];
    }
    std::cout << "\n";
  }

  // Print player info
  std::cout << "================================\n";
  std::cout << "Food items: " << state.food_items.size() << "\n";
  std::cout << "--------------------------------\n";
  for (const auto& [player_id, snake] : state.snakes) {
    int score = 0;
    auto score_it = state.scores.find(player_id);
    if (score_it != state.scores.end()) {
      score = score_it->second;
    }
    std::cout << player_id << ": " << (snake.alive ? "ALIVE" : "DEAD") << " | Score: " << score
              << " | Length: " << snake.length() << " | Head: (" << snake.head.x << "," << snake.head.y << ")\n";
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
