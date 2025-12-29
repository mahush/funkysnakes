#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>
#include <utility>

#include "snake/game_messages.hpp"

namespace snake {

// Forward declarations
struct GameState;

/**
 * @brief Lens to operate on direction command state with access to snakes
 *
 * Extracts direction_command_state and snakes from GameState, passes them to
 * an operation function along with forwarded arguments, then updates the command state.
 *
 * Example usage:
 *   state = over_direction_command_and_snakes(
 *       state,
 *       direction_command_filter::try_add,
 *       direction_cmd
 *   );
 *
 * @tparam Op Function type: (command_state, snakes, args...) -> updated_command_state
 * @tparam Args Additional argument types to forward
 * @param state Current game state
 * @param op Operation to apply
 * @param args Additional arguments to forward to op
 * @return Updated game state with modified command state
 */
template <typename Op, typename... Args>
GameState over_direction_command_and_snakes(GameState state, Op op, Args&&... args) {
  state.direction_command = op(state.direction_command, state.snakes, std::forward<Args>(args)...);
  return state;
}

/**
 * @brief Lens for consuming directions from command queues
 *
 * Applies a consume operation to direction command state and returns both the updated
 * state and the consumed directions.
 *
 * Example usage:
 *   auto [new_state, consumed] = over_direction_command_consuming(
 *       state,
 *       direction_command_filter::try_consume_next
 *   );
 *
 * @tparam Op Function type: (command_state) -> ConsumeResult
 * @param state Current game state
 * @param op Consume operation to apply
 * @return Pair of (updated state, consumed directions map)
 */
template <typename Op>
std::pair<GameState, std::map<PlayerId, Direction>> over_direction_command_consuming(GameState state, Op op) {
  auto result = op(state.direction_command);
  state.direction_command = result.filters;
  return {state, result.consumed_directions};
}

/**
 * @brief Lens to extract board and snakes from GameState and pass to operation
 *
 * Extracts board and snakes from GameState and passes them to the operation
 * along with any additional arguments.
 *
 * Example usage:
 *   Point pos = with_board_and_snakes(state, generateRandomFoodPosition);
 *
 * @tparam Op Function type: (Board, snakes, args...) -> Result
 * @tparam Args Additional argument types to forward
 * @param state Current game state
 * @param op Operation to apply
 * @param args Additional arguments to forward to op
 * @return Result of calling op with board, snakes, and additional args
 */
template <typename Op, typename... Args>
auto with_board_and_snakes(const GameState& state, Op op, Args&&... args) {
  return op(state.board, state.snakes, std::forward<Args>(args)...);
}

// ============================================================================
// Lenses for sub-state transformations (pass by value with move semantics)
// ============================================================================

/**
 * @brief Lens: Update snakes only
 *
 * Extracts snakes, applies operation, updates state with result.
 * Uses move semantics for efficiency.
 *
 * @tparam Op Function type: (snakes) -> snakes
 * @param state Current game state
 * @param op Operation to apply
 * @return Updated game state
 */
template <typename Op>
GameState over_snakes(GameState state, Op op) {
  state.snakes = op(std::move(state.snakes));
  return state;
}

/**
 * @brief Lens: Update snakes with access to board and food for read-only context
 *
 * Extracts snakes, passes board and food as read-only context.
 * Uses move semantics for snakes.
 *
 * @tparam Op Function type: (snakes, board, food_items) -> snakes
 * @param state Current game state
 * @param op Operation to apply
 * @return Updated game state
 */
template <typename Op>
GameState over_snakes_with_board_and_food(GameState state, Op op) {
  state.snakes = op(std::move(state.snakes), state.board, state.food_items);
  return state;
}

/**
 * @brief Lens: Update both snakes and scores together
 *
 * For operations that need to modify both snakes and scores atomically.
 * Operation returns tuple of (new_snakes, new_scores).
 *
 * @tparam Op Function type: (snakes, scores, board, mode) -> (snakes, scores)
 * @param state Current game state
 * @param op Operation to apply
 * @return Updated game state
 */
template <typename Op>
GameState over_snakes_and_scores(GameState state, Op op) {
  auto [new_snakes, new_scores] = op(std::move(state.snakes), std::move(state.scores), state.board,
                                      state.collision_mode);
  state.snakes = std::move(new_snakes);
  state.scores = std::move(new_scores);
  return state;
}

/**
 * @brief Lens: Update both food and scores with access to snakes
 *
 * For operations that modify food and scores while reading snake positions.
 * Operation returns tuple of (new_food, new_scores).
 *
 * @tparam Op Function type: (food, scores, snakes, board) -> (food, scores)
 * @param state Current game state
 * @param op Operation to apply
 * @return Updated game state
 */
template <typename Op>
GameState over_food_and_scores_with_snakes(GameState state, Op op) {
  auto [new_food, new_scores] = op(std::move(state.food_items), std::move(state.scores), state.snakes, state.board);
  state.food_items = std::move(new_food);
  state.scores = std::move(new_scores);
  return state;
}

/**
 * @brief Lens: Update food with access to snakes
 *
 * For operations that modify food while reading snake positions.
 *
 * @tparam Op Function type: (food, snakes) -> food
 * @param state Current game state
 * @param op Operation to apply
 * @return Updated game state
 */
template <typename Op>
GameState over_food_with_snakes(GameState state, Op op) {
  state.food_items = op(std::move(state.food_items), state.snakes);
  return state;
}

/**
 * @brief Lens: Update food with access to board and snakes for positioning
 *
 * For operations that modify food while reading board/snakes for position generation.
 * Supports additional forwarded arguments (e.g., tick_count).
 *
 * @tparam Op Function type: (food, board, snakes, args...) -> food
 * @tparam Args Additional argument types to forward
 * @param state Current game state
 * @param op Operation to apply
 * @param args Additional arguments to forward to op
 * @return Updated game state
 */
template <typename Op, typename... Args>
GameState over_food_with_board_and_snakes(GameState state, Op op, Args&&... args) {
  state.food_items = op(std::move(state.food_items), state.board, state.snakes, std::forward<Args>(args)...);
  return state;
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
