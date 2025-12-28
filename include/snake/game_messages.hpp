#pragma once

#include <deque>
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
 *
 * Also includes snake state: direction and alive status.
 * Note: player_id is NOT stored here - it's the key in GameState.snakes map
 */
struct Snake {
  Point head;
  std::vector<Point> tail;  // Can be empty
  Direction current_direction;
  bool alive{true};

  /**
   * @brief Create snake from head and optional tail
   */
  static Snake create(Point head, std::vector<Point> tail = {}, Direction dir = Direction::RIGHT,
                      bool is_alive = true) {
    return Snake{head, tail, dir, is_alive};
  }

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
 * @brief State for one player's direction command queue
 *
 * Buffers incoming direction commands and filters out invalid moves.
 * Queue size is limited to prevent unbounded growth from held keys.
 */
struct DirectionCommandFilterState {
  std::deque<Direction> queue;
  static constexpr size_t MAX_QUEUE_SIZE = 2;
};

/**
 * @brief Complete game state snapshot
 */
struct GameState {
  GameId game_id;
  std::map<PlayerId, Snake> snakes;                                   // Map from player ID to snake
  std::map<PlayerId, int> scores;                                     // Player scores
  std::vector<Point> food_items;                                      // Food items on the board
  std::map<PlayerId, DirectionCommandFilterState> direction_command;  // Direction input buffers
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
