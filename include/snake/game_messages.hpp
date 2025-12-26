#pragma once

#include <map>
#include <string>
#include <vector>

#include "snake/control_messages.hpp"

namespace snake {

/**
 * @brief Direction enumeration
 */
enum class Direction { UP, DOWN, LEFT, RIGHT };

/**
 * @brief Point in 2D space
 *
 * Represents a position on the game board.
 */
struct Point {
  int x;
  int y;

  bool operator==(const Point& other) const noexcept { return x == other.x && y == other.y; }
};

/**
 * @brief Snake with guaranteed head
 *
 * Represents a snake that always has at least a head.
 * The tail can be empty (snake of length 1).
 * This makes illegal states (empty snake) unrepresentable.
 */
struct Snake {
  Point head;
  std::vector<Point> tail;  // Can be empty

  /**
   * @brief Create snake from head and optional tail
   */
  static Snake create(Point head, std::vector<Point> tail = {}) { return Snake{head, tail}; }

  /**
   * @brief Get total length of snake
   */
  size_t length() const { return 1 + tail.size(); }

  /**
   * @brief Convert to body representation (vector of points with head at index 0)
   *
   * This is mainly for compatibility/serialization purposes.
   */
  std::vector<Point> toBody() const {
    std::vector<Point> body;
    body.reserve(1 + tail.size());
    body.push_back(head);
    body.insert(body.end(), tail.begin(), tail.end());
    return body;
  }
};

/**
 * @brief Snake state for a single player
 *
 * Note: player_id is NOT stored here - it's the key in GameState.snakes map
 */
struct SnakeState {
  Snake snake;
  Direction current_direction;
  bool alive{true};
};

/**
 * @brief Complete game state snapshot
 */
struct GameState {
  GameId game_id;
  std::map<PlayerId, SnakeState> snakes;  // Map from player ID to snake state
  std::map<PlayerId, int> scores;         // Player scores
  std::vector<Point> food_items;          // Food items on the board
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
