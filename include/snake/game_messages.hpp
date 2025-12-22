#pragma once

#include <string>
#include <vector>

#include "snake/control_messages.hpp"

namespace snake {

/**
 * @brief Direction enumeration
 */
enum class Direction { UP, DOWN, LEFT, RIGHT };

/**
 * @brief Position on the game board
 */
struct Position {
  int x;
  int y;

  bool operator==(const Position& other) const { return x == other.x && y == other.y; }
};

/**
 * @brief Snake state for a single player
 */
struct SnakeState {
  PlayerId player_id;
  std::vector<Position> body;  // Head is at index 0
  Direction current_direction;
  int score{0};
  bool alive{true};
};

/**
 * @brief Complete game state snapshot
 */
struct GameState {
  GameId game_id;
  std::vector<SnakeState> snakes;
  Position food;
  int level{1};
  int board_width{60};
  int board_height{20};
};

/**
 * @brief Game tick event - drives the game loop
 */
struct Tick {
  GameId game_id;
};

/**
 * @brief Player direction change command
 */
struct DirectionChange {
  GameId game_id;
  PlayerId player_id;
  Direction new_direction;
};

/**
 * @brief Game state update notification
 */
struct StateUpdate {
  GameState state;
};

/**
 * @brief Level change notification
 */
struct LevelChange {
  GameId game_id;
  int new_level;
};

/**
 * @brief Game over notification
 */
struct GameOver {
  GameSummary summary;
};

/**
 * @brief Tick rate change command
 */
struct TickRateChange {
  GameId game_id;
  int interval_ms;  // New tick interval in milliseconds
};

/**
 * @brief Start clock command
 */
struct StartClock {
  GameId game_id;
  int interval_ms;
};

/**
 * @brief Stop clock command
 */
struct StopClock {
  GameId game_id;
};

/**
 * @brief User input event (from keyboard/controller)
 */
struct UserInputEvent {
  PlayerId player_id;
  char key;  // For now, simple char input
};

}  // namespace snake
