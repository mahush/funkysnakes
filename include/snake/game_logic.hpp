#pragma once

#include <tuple>
#include <vector>

#include "snake/game_types.hpp"
#include "snake/utility.hpp"

namespace snake {

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
// Snake Game Logic
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

// ============================================================================
// Food Logic
// ============================================================================

/**
 * @brief Generate a random food position not occupied by snakes
 *
 * @param board Board dimensions
 * @param snakes Map of player snakes to check for collisions
 * @param random_int Function that generates random int in range [min, max]
 * @return Random unoccupied position (or random position if all attempts fail)
 */
Point generateRandomFoodPosition(const Board& board, const PerPlayerSnakes& snakes, RandomIntGeneratorFn random_int);

/**
 * @brief Add cut tail segments as food
 *
 * @param food_items Food (by value)
 * @param cut_tails Cut tail segments to add
 * @return Updated food with cut tails added
 */
FoodItems dropCutTailsAsFood(FoodItems food_items, const FoodItems& cut_tails);

/**
 * @brief Add dead snake bodies to food (for BITE_DROP_FOOD mode)
 *
 * @param food_items Food (by value)
 * @param snakes Snakes (to check for dead ones)
 * @return Updated food with dead snake bodies added
 */
FoodItems dropDeadSnakesAsFood(FoodItems food_items, const PerPlayerSnakes& snakes);

/**
 * @brief Handle snakes eating food
 *
 * If snake head is on food:
 * - Remove eaten food
 * - Award points (+10)
 *
 * @param food_items Food (by value)
 * @param scores Scores (by value)
 * @param snakes Snakes (to check head positions)
 * @return Tuple of (updated food, updated scores)
 */
std::tuple<FoodItems, PerPlayerScores> handleFoodEating(FoodItems food_items,
                                                        PerPlayerScores scores,
                                                        const PerPlayerSnakes& snakes);

/**
 * @brief Initialize food items to a target count
 *
 * Parameter order: bound parameters first (for bindFront), then lens parameters.
 *
 * @param random_int Random number generator function
 * @param count Number of food items to create
 * @param food_items Current food items (unused - for lens compatibility)
 * @param board Board dimensions
 * @param snakes Snakes (for position generation)
 * @return Food items list with specified count
 */
FoodItems initializeFood(RandomIntGeneratorFn random_int,
                         int count,
                         FoodItems food_items,
                         const Board& board,
                         const PerPlayerSnakes& snakes);

/**
 * @brief Replenish food to maintain target count
 *
 * @param random_int Random number generator function
 * @param target_count Desired number of food items
 * @param food_items Food (by value)
 * @param board Board dimensions
 * @param snakes Snakes (for position generation)
 * @return Updated food with new items added
 */
FoodItems replenishFood(RandomIntGeneratorFn random_int,
                        int target_count,
                        FoodItems food_items,
                        const Board& board,
                        const PerPlayerSnakes& snakes);

/**
 * @brief Reposition one random food item
 *
 * Parameter order: bound parameters first (for bindFront), then lens parameters.
 *
 * @param random_int Random number generator function
 * @param food_items Food (by value)
 * @param board Board dimensions
 * @param snakes Snakes (for position generation)
 * @return Updated food with one item repositioned
 */
FoodItems repositionRandomFood(RandomIntGeneratorFn random_int,
                               FoodItems food_items,
                               const Board& board,
                               const PerPlayerSnakes& snakes);

}  // namespace snake
