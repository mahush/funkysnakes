#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>
#include <utility>

#include "snake/game_messages.hpp"
#include "snake/generic_lens.hpp"

namespace snake {

// Forward declarations
struct GameState;

/**
 * @brief Lens to operate on pending direction queues with access to snakes
 *
 * NOTE: This is a non-decorator style for backwards compatibility.
 * Prefer using the generic lens directly in new code.
 *
 * Example usage:
 *   state = over_pending_directions_and_snakes(
 *       state,
 *       direction_command_filter::try_add,
 *       direction_cmd
 *   );
 *
 * @tparam Op Function type: (pending_directions, snakes, args...) -> updated_pending_directions
 * @tparam Args Additional argument types to forward
 * @param state Current game state
 * @param op Operation to apply
 * @param args Additional arguments to forward to op
 * @return Updated game state with modified pending directions
 */
template <typename Op, typename... Args>
GameState over_pending_directions_and_snakes(GameState state, Op op, Args&&... args) {
  auto transform = lens(mutate<&GameState::pending_directions>,
                       read<&GameState::snakes>,
                       std::move(op));
  return transform(std::move(state), std::forward<Args>(args)...);
}

/**
 * @brief Lens decorator: Update pending direction queues
 *
 * Returns a state transformer that applies an operation to pending direction queues.
 *
 * Example usage:
 *   auto transformer = over_pending_directions(direction_command_filter::try_consume_next);
 *   auto [new_state, next_dirs] = transformer(state);
 *
 * @tparam Op Function type: (pending_directions) -> pending_directions or tuple<pending_directions, ...>
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState or tuple<GameState, ...>
 */
template <typename Op>
auto over_pending_directions(Op op) {
  return lens(mutate<&GameState::pending_directions>, read<>, std::move(op));
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
// Lens Decorators - Built on generic lens for consistent behavior
// ============================================================================
// Each lens is now a thin wrapper around the generic lens implementation.
// This provides:
// - Consistent handling of mutable/readonly fields
// - Automatic pipeline argument forwarding
// - Automatic threading of additional outputs
// - Reduced code duplication

/**
 * @brief Lens decorator: Update snakes only
 *
 * Returns a decorator that accepts GameState and additional arguments,
 * forwards them to the operation, and returns updated state.
 *
 * @tparam Op Function type: (snakes, args...) -> snakes
 * @param op Operation to apply to snakes
 * @return State transformer: (GameState, args...) -> GameState
 */
template <typename Op>
auto over_snakes(Op op) {
  return lens(mutate<&GameState::snakes>, read<>, std::move(op));
}

/**
 * @brief Lens decorator: Update snakes with access to board and food
 *
 * @tparam Op Function type: (snakes, board, food_items) -> snakes
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState
 */
template <typename Op>
auto over_snakes_with_board_and_food(Op op) {
  return lens(mutate<&GameState::snakes>,
              read<&GameState::board, &GameState::food_items>,
              std::move(op));
}

/**
 * @brief Lens decorator: Update both snakes and scores together
 *
 * Handles both simple and extended operations:
 * - Op returns (snakes, scores) → returns GameState
 * - Op returns (snakes, scores, additional_result) → returns (GameState, additional_result)
 *
 * The additional result (if any) is threaded through the pipeline automatically.
 *
 * @tparam Op Function type: (snakes, scores) -> (snakes, scores[, additional_result])
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState or (GameState, additional_result)
 */
template <typename Op>
auto over_snakes_and_scores(Op op) {
  return lens(mutate<&GameState::snakes, &GameState::scores>,
              read<>,
              std::move(op));
}

/**
 * @brief Lens decorator: Update both food and scores with access to snakes
 *
 * @tparam Op Function type: (food, scores, snakes, board) -> (food, scores)
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState
 */
template <typename Op>
auto over_food_and_scores_with_snakes(Op op) {
  return lens(mutate<&GameState::food_items, &GameState::scores>,
              read<&GameState::snakes, &GameState::board>,
              std::move(op));
}

/**
 * @brief Lens decorator: Update food
 *
 * The operation can accept food and any number of additional parameters.
 * Use this when the additional parameters come from the pipeline (e.g., cut tails).
 *
 * @tparam Op Function type: (food, args...) -> food
 * @param op Operation to apply
 * @return State transformer: (GameState, args...) -> GameState
 */
template <typename Op>
auto over_food(Op op) {
  return lens(mutate<&GameState::food_items>, read<>, std::move(op));
}

/**
 * @brief Lens decorator: Update food with access to snakes
 *
 * @tparam Op Function type: (food, snakes) -> food
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState
 */
template <typename Op>
auto over_food_with_snakes(Op op) {
  return lens(mutate<&GameState::food_items>,
              read<&GameState::snakes>,
              std::move(op));
}

/**
 * @brief Lens decorator: Update food with access to board and snakes
 *
 * @tparam Op Function type: (food, board, snakes) -> food
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState
 */
template <typename Op>
auto over_food_with_board_and_snakes(Op op) {
  return lens(mutate<&GameState::food_items>,
              read<&GameState::board, &GameState::snakes>,
              std::move(op));
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
