#include "snake/renderer_actor.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "actor-core/timer/timer.hpp"
#include "snake/control_messages.hpp"
#include "snake/process_helpers.hpp"

namespace snake {

using actor_core::make_cancel_command;
using actor_core::make_periodic_command;

RendererActor::RendererActor(ActorContext ctx, TopicPtr<RenderableStateMsg> state_topic,
                             TopicPtr<GameOverMsg> gameover_topic, TopicPtr<GameStateMetadataMsg> metadata_topic,
                             TimerFactoryPtr timer_factory)
    : Actor(ctx),
      state_sub_(create_sub(state_topic)),
      gameover_sub_(create_sub(gameover_topic)),
      metadata_sub_(create_sub(metadata_topic)),
      flash_timer_(create_timer<FlashTimer>(timer_factory)) {}

void RendererActor::processInputs() {
  // Process all pending state updates
  while (auto msg = state_sub_->tryTakeMessage()) {
    onRenderableState(*msg);
  }

  // Process game over messages
  while (auto msg = gameover_sub_->tryTakeMessage()) {
    onGameOver(*msg);
  }

  // Process game state metadata (level, pause state)
  while (auto msg = metadata_sub_->tryTakeMessage()) {
    onGameStateMetadata(*msg);
  }

  // Process flash timer events
  process_event(flash_timer_, [&](const FlashTimerElapsedEvent&) { onFlashTimer(); });
}

void RendererActor::onRenderableState(const RenderableStateMsg& state) {
  last_state_ = state;
  renderBoard(state, false, metadata_.paused);
}

void RendererActor::onGameStateMetadata(const GameStateMetadataMsg& msg) {
  metadata_ = msg;
  // Re-render if we have state (to show updated level/pause state)
  if (!last_state_.snakes.empty()) {
    renderBoard(last_state_, game_over_active_, metadata_.paused);
  }
}

void RendererActor::renderBoard(const RenderableStateMsg& state, bool show_game_over, bool show_paused) {
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
  std::string game_text = "Game: " + metadata_.game_id;
  std::string level_text = "Level: " + std::to_string(metadata_.level);
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

  // Overlay "GAME OVER" ASCII art if game over and flash is visible
  if (show_game_over && flash_visible_) {
    // ASCII art for "GAME OVER" (5 lines, ~50 chars wide)
    // clang-format off
    std::vector<std::string> game_over_text = {
        " ####    ##   #   # ####     ####  #   # #### ####  ",
        "#       #  #  ## ## #       #    # #   # #    #   # ",
        "#  ##  ###### # # # ###     #    # #   # ###  ####  ",
        "#   #  #    # #   # #       #    #  # #  #    #  #  ",
        " ####  #    # #   # ####     ####    #   #### #   # "
    };
    // clang-format on

    // Calculate starting position to center the text
    int text_height = game_over_text.size();
    int text_width = game_over_text[0].length();
    int start_y = (state.board.height - text_height) / 2;
    int start_x = (state.board.width - text_width) / 2;

    // Add padding for the background box
    int padding = 2;
    int box_start_y = start_y - padding;
    int box_end_y = start_y + text_height + padding - 1;
    int box_start_x = start_x - padding;
    int box_end_x = start_x + text_width + padding - 1;

    // First, draw a background box (clear the area)
    for (int y = box_start_y; y <= box_end_y; ++y) {
      if (y >= 0 && y < state.board.height) {
        for (int x = box_start_x; x <= box_end_x; ++x) {
          if (x >= 0 && x < state.board.width) {
            board[y][x] = ' ';
          }
        }
      }
    }

    // Then overlay the text on top
    for (size_t i = 0; i < game_over_text.size(); ++i) {
      int y = start_y + i;
      if (y >= 0 && y < state.board.height) {
        for (size_t j = 0; j < game_over_text[i].length(); ++j) {
          int x = start_x + j;
          if (x >= 0 && x < state.board.width && game_over_text[i][j] != ' ') {
            board[y][x] = game_over_text[i][j];
          }
        }
      }
    }
  }

  // Overlay pause symbol if game is paused
  if (show_paused) {
    // Pause symbol: two vertical bars using ASCII
    // clang-format off
    std::vector<std::string> pause_symbol = {
        "|||||||  |||||||",
        "|||||||  |||||||",
        "|||||||  |||||||",
        "|||||||  |||||||",
        "|||||||  |||||||",
        "|||||||  |||||||",
        "|||||||  |||||||"
    };
    // clang-format on

    // Calculate starting position to center the symbol
    int symbol_height = pause_symbol.size();
    int symbol_width = pause_symbol[0].length();
    int start_y = (state.board.height - symbol_height) / 2;
    int start_x = (state.board.width - symbol_width) / 2;

    // Add padding for the background box (horizontal more than vertical)
    int padding_horizontal = 3;
    int padding_vertical = 1;
    int box_start_y = start_y - padding_vertical;
    int box_end_y = start_y + symbol_height + padding_vertical - 1;
    int box_start_x = start_x - padding_horizontal;
    int box_end_x = start_x + symbol_width + padding_horizontal - 1;

    // First, draw a background box (clear the area)
    for (int y = box_start_y; y <= box_end_y; ++y) {
      if (y >= 0 && y < state.board.height) {
        for (int x = box_start_x; x <= box_end_x; ++x) {
          if (x >= 0 && x < state.board.width) {
            board[y][x] = ' ';
          }
        }
      }
    }

    // Then overlay the pause symbol on top
    for (size_t i = 0; i < pause_symbol.size(); ++i) {
      int y = start_y + i;
      if (y >= 0 && y < state.board.height) {
        for (size_t j = 0; j < pause_symbol[i].length(); ++j) {
          int x = start_x + j;
          if (x >= 0 && x < state.board.width && pause_symbol[i][j] != ' ') {
            board[y][x] = pause_symbol[i][j];
          }
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

void RendererActor::onGameOver(const GameOverMsg& /* msg */) {
  game_over_active_ = true;
  flash_visible_ = true;

  // Start flash timer (750ms interval for flashing effect)
  flash_timer_->execute_command(make_periodic_command<FlashTimerTag>(std::chrono::milliseconds(750)));

  // Re-render the last state with "GAME OVER" overlay
  renderBoard(last_state_, true, metadata_.paused);
}

void RendererActor::onFlashTimer() {
  if (!game_over_active_) {
    return;
  }

  // Toggle flash visibility
  flash_visible_ = !flash_visible_;

  // Re-render with current flash state
  renderBoard(last_state_, true, metadata_.paused);
}

}  // namespace snake
