#pragma once

#include "snake/game_messages.hpp"
#include "snake/utility.hpp"

namespace snake {
namespace classic_arena_authority {

/**
 * @brief Classic Arena Authority - owns the valid evolution of the classic arena
 *
 * The arena consists of snakes, scores, food and buffered steering intentions on a board.
 * This Authority owns the complete arena transition including its meaningful sequencing:
 * steering consumption, movement, collisions, food drops, eating, replenishment and
 * repositioning. Nested Authorities own local evolution: snake_model (body and life of
 * one snake) and direction_command_filter (buffered steering intentions).
 *
 * Out of scope: game lifecycle, pause, level progression and tick cadence. These are
 * owned outside the arena and reach it only as inputs (when ticks happen, reposition
 * requests) or by reading its projections (alive states).
 */

// Minimum number of food items kept on the board
constexpr int MIN_FOOD_COUNT = 5;

/**
 * @brief Create the initial classic arena
 *
 * Places Player A and Player B and spawns the initial food.
 *
 * @param random_int Random number source for food placement
 * @param state State to initialize the arena in
 * @return State with the initial arena
 */
GameState initial(const RandomIntGeneratorFn& random_int, GameState state);

/**
 * @brief Register a player's steering intention
 *
 * @param state Current state
 * @param cmd Steering command from a player
 * @return State with updated steering intentions
 */
GameState steer(GameState state, const DirectionCommand& cmd);

/**
 * @brief Request that one food item is repositioned on the next tick
 *
 * @param state Current state
 * @return State with a pending reposition request
 */
GameState requestFoodReposition(GameState state);

/**
 * @brief Advance the arena by one step
 *
 * @param random_int Random number source for food placement
 * @param state Current state
 * @return State after one arena step
 */
GameState tick(const RandomIntGeneratorFn& random_int, GameState state);

}  // namespace classic_arena_authority
}  // namespace snake
