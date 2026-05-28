#ifndef SNAKE_FOOD_OPERATIONS_HPP
#define SNAKE_FOOD_OPERATIONS_HPP

#include <tuple>
#include <vector>

#include "snake/game_types.hpp"
#include "snake/utility.hpp"

namespace snake {

// ============================================================================
// Food Position Generation
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

// ============================================================================
// Food Transformation Operations
// ============================================================================

/**
 * @brief Add cut tail segments as food
 *
 * Takes cut tail segments and adds them as food items.
 * Separate stage for modular composition.
 *
 * @param food_items Food (by value)
 * @param cut_tails Cut tail segments to add
 * @return Updated food with cut tails added
 */
FoodItems dropCutTailsAsFood(FoodItems food_items, const FoodItems& cut_tails);

/**
 * @brief Add dead snake bodies to food (for BITE_DROP_FOOD mode)
 *
 * Checks for dead snakes and adds their body segments as food.
 * Separate from collision detection to maintain sub-state separation.
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
 * Creates food items from scratch (starting with empty list).
 * Thin wrapper around replenishFood for semantic clarity.
 *
 * Parameter order: bound parameters first (for bindFront), then lens parameters (food, board, snakes).
 * Note: food_items parameter is unused since we initialize from scratch.
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
 * Adds new food items until the target count is reached.
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
 * Moves one randomly selected food item to a new random position.
 * Does nothing if food list is empty.
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

#endif  // SNAKE_FOOD_OPERATIONS_HPP
