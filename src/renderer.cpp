#include "snake/renderer.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "snake/control_messages.hpp"

namespace snake {

Renderer::Renderer(Actor<Renderer>::ActorContext ctx, TopicPtr<RenderableState> state_topic,
                   TopicPtr<GameOver> gameover_topic, TopicPtr<LevelChange> level_topic)
    : Actor(ctx),
      state_sub_(create_sub(state_topic)),
      gameover_sub_(create_sub(gameover_topic)),
      level_sub_(create_sub(level_topic)) {}

void Renderer::processMessages() {
  // Process all pending state updates
  while (auto msg = state_sub_->tryReceive()) {
    onRenderableState(*msg);
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

void Renderer::onRenderableState(const RenderableState& state) {
  // Clear screen (simple version - just add newlines)
  std::cout << "\n\n";

  // Calculate separator width based on board width
  int board_width = state.board.width;
  int separator_width = board_width;  // Match the board width

  // Create separator line with box drawing characters
  std::string separator;
  for (int i = 0; i < separator_width; ++i) {
    separator += "═";
  }

  // Header box with title
  std::string title = " SNAKE GAME ";
  int padding_left = (separator_width - title.length()) / 2;
  int padding_right = separator_width - title.length() - padding_left;
  std::string title_padding_left;
  std::string title_padding_right;
  for (int i = 0; i < padding_left; ++i) {
    title_padding_left += "═";
  }
  for (int i = 0; i < padding_right; ++i) {
    title_padding_right += "═";
  }
  std::cout << "╔" << title_padding_left << title << title_padding_right << "╗\n";

  // Create info line with left-aligned game ID and right-aligned level
  std::string game_text = "Game: " + state.game_id;
  std::string level_text = "Level: " + std::to_string(state.level);
  int padding = separator_width - game_text.length() - level_text.length();

  std::cout << "║" << game_text << std::string(padding, ' ') << level_text << "║\n";
  std::cout << "╚" << separator << "╝\n";

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
        board[head.y][head.x] = (player_id == PLAYER_A) ? 'A' : 'B';
      }

      // Draw tail
      for (const Point& segment : snake.tail) {
        if (segment.y >= 0 && segment.y < state.board.height && segment.x >= 0 && segment.x < state.board.width) {
          board[segment.y][segment.x] = (player_id == PLAYER_A) ? 'a' : 'b';
        }
      }
    }
  }

  // Print board with box borders
  std::string horizontal_line;
  for (int i = 0; i < board_width; ++i) {
    horizontal_line += "─";
  }
  std::cout << "┌" << horizontal_line << "┐\n";
  for (int y = 0; y < state.board.height; ++y) {
    std::cout << "│";
    for (int x = 0; x < state.board.width; ++x) {
      std::cout << board[y][x];
    }
    std::cout << "│\n";
  }
  std::cout << "└" << horizontal_line << "┘\n";

  // Print player info in a box with header
  // Column widths: Player(10) State(5) Score(5) Length(6) Head(rest)
  std::cout << "╔" << separator << "╗\n";
  std::cout << "║" << std::left << std::setw(10) << "Player" << " │ " << std::setw(5) << "State"
            << " │ " << std::right << std::setw(5) << "Score"
            << " │ " << std::setw(6) << "Length"
            << " │ " << std::left << std::setw(22) << "Head"
            << "║\n";
  // Create separator: 10+3+5+3+5+3+6+3+22 = 60
  std::cout << "╟───────────┼───────┼───────┼────────┼───────────────────────╢\n";
  for (const auto& [player_id, snake] : state.snakes) {
    int score = 0;
    auto score_it = state.scores.find(player_id);
    if (score_it != state.scores.end()) {
      score = score_it->second;
    }
    std::cout << "║";
    std::cout << std::left << std::setw(10) << player_id << " │ " << std::setw(5) << (snake.alive ? "ALIVE" : "DEAD")
              << " │ " << std::right << std::setw(5) << score << " │ " << std::setw(6) << snake.length() << " │ "
              << std::setw(2) << snake.head.x << "," << std::left << std::setw(2) << snake.head.y
              << std::string(17, ' ');
    std::cout << "║\n";
  }
  std::cout << "╚" << separator << "╝\n";
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
