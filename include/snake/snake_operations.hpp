#ifndef SNAKE_SNAKE_OPERATIONS_HPP
#define SNAKE_SNAKE_OPERATIONS_HPP

#include <tuple>
#include <vector>

#include "snake/game_types.hpp"
#include "snake/snake_model.hpp"

namespace snake {

// ============================================================================
// Basic Snake Utilities
// ============================================================================

/**
 * @brief Get next head position based on current direction
 */
Point getNextHeadPosition(const Snake& snake);

/**
 * @brief Wrap a point around board boundaries
 */
Point wrapPoint(Point p, const Board& board);

// ============================================================================
// Snake Transformations
// ============================================================================

/**
 * @brief Move snake one step in current direction (shortens tail)
 *
 * Calculates new head position, wraps it around board boundaries,
 * and removes the last tail segment.
 *
 * @param snake Snake to move
 * @param board Board dimensions for wrapping
 * @return Snake after moving
 */
Snake moveSnake(Snake snake, const Board& board);

/**
 * @brief Grow snake one step in current direction (keeps tail)
 *
 * Calculates new head position, wraps it around board boundaries,
 * and keeps all tail segments (snake grows by one).
 *
 * @param snake Snake to grow
 * @param board Board dimensions for wrapping
 * @return Snake after growing
 */
Snake growSnake(Snake snake, const Board& board);

/**
 * @brief Cut snake tail at specified point
 *
 * Removes all tail segments from the cut point onwards and returns them separately.
 * If cut point is the head or not in tail, snake is unchanged and cut is empty.
 *
 * @param snake Snake to cut
 * @param cut_point Point where to cut the tail
 * @return Tuple of (snake with tail cut, cut tail segments)
 */
std::tuple<Snake, std::vector<Point>> cutSnakeTailAt(Snake snake, Point cut_point);

// ============================================================================
// Player Initialization
// ============================================================================

/**
 * @brief Add a new player to the game
 *
 * Creates a snake at the specified position, initializes score to 0,
 * and sets up an empty direction queue for the player.
 *
 * Parameter order: bound parameters first (for bindFront compatibility),
 * then state parameters (snakes, scores, pending_directions).
 *
 * @param player_id Player identifier
 * @param start_position Starting position for snake head
 * @param initial_direction Initial movement direction
 * @param snake_length Initial snake length
 * @param snakes Current snakes map (by value)
 * @param scores Current scores map (by value)
 * @param pending_directions Current direction queues (by value)
 * @return Tuple of (updated snakes, updated scores, updated direction queues)
 */
std::tuple<PerPlayerSnakes, PerPlayerScores, PerPlayerDirectionQueue> addPlayer(PlayerId player_id,
                                                                                  Point start_position,
                                                                                  Direction initial_direction,
                                                                                  int snake_length,
                                                                                  PerPlayerSnakes snakes,
                                                                                  PerPlayerScores scores,
                                                                                  PerPlayerDirectionQueue pending_directions);

// ============================================================================
// Game Logic Operations on Snakes
// ============================================================================

/**
 * @brief Apply direction changes to snakes
 *
 * @param snakes Snake map (passed by value)
 * @param consumed_directions Directions consumed from input queues
 * @return Updated snakes with new directions
 */
PerPlayerSnakes applyDirectionChanges(PerPlayerSnakes snakes, const PerPlayerDirection& consumed_directions);

/**
 * @brief Move all alive snakes one step
 *
 * If snake head will land on food, grow (keep tail).
 * Otherwise, move (shorten tail).
 *
 * @param snakes Snakes to move (by value)
 * @param board Board dimensions (for wrapping)
 * @param food_items Food positions (to check if eating)
 * @return Updated snakes after movement
 */
PerPlayerSnakes moveSnakes(PerPlayerSnakes snakes, const Board& board, const std::vector<Point>& food_items);

/**
 * @brief Handle snake-to-snake collisions
 *
 * Updates snakes (kill/cut) and scores based on collision detection.
 * Returns cut tail segments for potential food conversion.
 *
 * @param snakes Snakes (by value)
 * @param scores Scores (by value)
 * @return Tuple of (updated snakes, updated scores, cut tail segments)
 */
std::tuple<PerPlayerSnakes, PerPlayerScores, std::vector<Point>> handleCollisions(PerPlayerSnakes snakes,
                                                                                   PerPlayerScores scores);

}  // namespace snake

#endif  // SNAKE_SNAKE_OPERATIONS_HPP
