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
 * @brief Extract alive states from snakes
 *
 * Transforms per-player snakes into per-player alive states.
 *
 * @param snakes Per-player snakes
 * @return Per-player alive states
 */
PerPlayerAliveStates extractAliveStates(const PerPlayerSnakes& snakes);

/**
 * @brief Get next head position based on current direction
 */
Point getNextHeadPosition(const Snake& snake);

/**
 * @brief Wrap a point around board boundaries
 */
Point wrapPoint(Point p, const Board& board);

/**
 * @brief Check if snake's head collides with its own tail
 *
 * @param snake Snake to check
 * @return true if head is in tail (self-bite)
 */
bool snakeBitesItself(const Snake& snake);

// ============================================================================
// Snake Transformations
// ============================================================================

/**
 * @brief Move snake one step in current direction
 *
 * Calculates new head position, wraps it around board boundaries.
 * If eating, keeps all tail segments (snake grows by one).
 * If not eating, removes the last tail segment (maintains length).
 *
 * @param snake Snake to move
 * @param board Board dimensions for wrapping
 * @param is_eating Whether snake is eating food (grows if true)
 * @return Snake after moving
 */
Snake moveSnake(Snake snake, const Board& board, bool is_eating);

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
 * Creates a snake at the specified position and initializes score to 0.
 *
 * Parameter order: bound parameters first (for bindFront compatibility),
 * then state parameters (snakes, scores).
 *
 * @param player_id Player identifier
 * @param start_position Starting position for snake head
 * @param initial_direction Initial movement direction
 * @param snake_length Initial snake length
 * @param snakes Current snakes map (by value)
 * @param scores Current scores map (by value)
 * @return Tuple of (updated snakes, updated scores)
 */
std::tuple<PerPlayerSnakes, PerPlayerScores> addPlayer(PlayerId player_id, Point start_position,
                                                        Direction initial_direction, int snake_length,
                                                        PerPlayerSnakes snakes, PerPlayerScores scores);

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
PerPlayerSnakes moveSnakes(PerPlayerSnakes snakes, const Board& board, const FoodItems& food_items);

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
