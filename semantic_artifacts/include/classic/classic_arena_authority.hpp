#pragma once

#include "classic/arena_events.hpp"
#include "classic/direction_command_filter.hpp"
#include "snake/game_types.hpp"
#include "snake/utility.hpp"

namespace snake {
namespace classic_arena_authority {

/**
 * Classic Arena Authority - owns the valid evolution of the classic arena
 *
 * The arena consists of snakes, scores, food and buffered steering intentions on a board.
 * This Authority owns the complete arena transition including its meaningful sequencing:
 * steering consumption, movement, collisions, food drops, eating, replenishment and
 * repositioning. Nested Authorities own local evolution: snake_model (body and life of
 * one snake) and direction_command_filter (buffered steering intentions).
 *
 * Out of scope: game lifecycle, pause, level progression and tick cadence. These are
 * owned outside the arena and reach it only as inputs (when ticks happen, reposition
 * requests) or by reading its state (alive states).
 */

// Minimum number of food items kept on the board
constexpr int MIN_FOOD_COUNT = 5;

/**
 * @brief Arena state owned by the Classic Arena Authority
 *
 * Fields may be read freely. Changes must go through the transitions below only;
 * callers must not modify fields or compose their own sequences of game-rule helpers.
 */
struct State {
  Board board;                                                     // Board dimensions
  PerPlayerSnakes snakes;                                          // Snakes for each player
  PerPlayerScores scores;                                          // Scores for each player
  FoodItems food_items;                                            // Food items on the board
  direction_command_filter::State direction_command_filter_state;  // Buffered steering intentions
  CollisionMode collision_mode{CollisionMode::BITE_DROP_FOOD};     // Collision handling mode
  bool should_reposition_food{false};                              // Reposition requested for next step
  ArenaEvents events;                                              // What happened during the last step
};

/**
 * @brief Create the initial classic arena
 *
 * Places Player A and Player B and spawns the initial food.
 *
 * @param random_int Random number source for food placement
 * @param board Board dimensions
 * @return Initial arena state
 */
State initial(const RandomIntGeneratorFn& random_int, Board board);

/**
 * @brief Register a player's steering intention
 *
 * @param state Current arena state
 * @param cmd Steering command from a player
 * @return Arena state with updated steering intentions
 */
State steer(State state, const DirectionCommand& cmd);

/**
 * @brief Request that one food item is repositioned on the next step
 *
 * @param state Current arena state
 * @return Arena state with a pending reposition request
 */
State requestFoodReposition(State state);

/**
 * @brief Advance the arena by one step
 *
 * Arena rules report what happened as events; the Scoring Policy turns them into score
 * changes at the end of the step. The events stay readable in the resulting state.
 *
 * Parameter order: bound parameters first (for bindFront), then state.
 *
 * @param random_int Random number source for food placement
 * @param state Current arena state
 * @return Arena state after one step
 */
State tick(const RandomIntGeneratorFn& random_int, State state);

}  // namespace classic_arena_authority
}  // namespace snake
