#ifndef SNAKE_SNAKE_OPERATIONS_HPP
#define SNAKE_SNAKE_OPERATIONS_HPP

#include <tuple>
#include <vector>

#include "snake/game_types.hpp"
#include "snake/snake_model.hpp"

namespace snake {

using Snake = snake_model::Snake;

// ============================================================================
// Single-Snake Predicates
// ============================================================================

/**
 * @brief Check if snake's head collides with its own tail
 *
 * @param snake Snake to check
 * @return true if head is in tail (self-bite)
 */
bool snakeBitesItself(const Snake& snake);

// ============================================================================
// Inter-Snake Predicates
// ============================================================================

/**
 * @brief Check if first snake's head bites second snake's body
 *
 * Returns true if the first snake's head is contained anywhere in the
 * second snake's body (head or tail).
 *
 * @param first Snake that might be biting
 * @param second Snake that might be bitten
 * @return true if first bites second
 */
bool firstBitesSecond(const Snake& first, const Snake& second);

/**
 * @brief Check if both snakes bite each other simultaneously
 *
 * @param a First snake
 * @param b Second snake
 * @return true if both bite each other
 */
bool bothBiteEachOther(const Snake& a, const Snake& b);

// ============================================================================
// Batch Utilities
// ============================================================================

/**
 * @brief Extract alive states from snakes
 *
 * @param snakes Per-player snakes
 * @return Per-player alive states
 */
PerPlayerAliveStates extractAliveStates(const PerPlayerSnakes& snakes);

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
std::tuple<PerPlayerSnakes, PerPlayerScores> addPlayer(PlayerId player_id,
                                                       Point start_position,
                                                       Direction initial_direction,
                                                       int snake_length,
                                                       PerPlayerSnakes snakes,
                                                       PerPlayerScores scores);

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
PerPlayerSnakes applyDirectionMsgs(PerPlayerSnakes snakes, const PerPlayerDirection& consumed_directions);

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
