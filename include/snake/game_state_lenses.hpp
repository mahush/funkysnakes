#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>
#include <utility>

#include "snake/game_messages.hpp"
#include "snake/generic_lens.hpp"

namespace snake {

// Forward declarations
struct GameState;

// ============================================================================
// Lens Decorators - Built on generic lens for consistent behavior
// ============================================================================
// Lenses are state transformers that follow the `over_*` naming pattern.
// Each lens is a thin wrapper around the generic lens implementation.
// This provides:
// - Consistent handling of mutable/readonly fields
// - Automatic pipeline argument forwarding
// - Automatic threading of additional outputs
// - Reduced code duplication
//
// Usage pattern:
//   auto transformer = over_snakes(moveSnakes);  // Create transformer
//   new_state = transformer(state);              // Apply to state

/**
 * @brief Lens decorator: Update direction command filter state
 *
 * Returns a state transformer that applies an operation to direction_command_filter module state.
 *
 * Example usage:
 *   auto transformer = over_direction_command_filter_state(direction_command_filter::try_consume_next);
 *   auto [new_state, next_dirs] = transformer(state);
 *
 * @tparam Op Function type: (state) -> state or tuple<state, ...>
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState or tuple<GameState, ...>
 */
template <typename Op>
auto over_direction_command_filter_state(Op op) {
  return lens(mutate<&GameState::direction_command_filter_state>, read<>, std::move(op));
}

/**
 * @brief Lens decorator: Update direction command filter state with access to snakes
 *
 * @tparam Op Function type: (direction_command_filter_state, snakes, args...) -> direction_command_filter_state
 * @param op Operation to apply
 * @return State transformer: (GameState, args...) -> GameState
 */
template <typename Op>
auto over_direction_command_filter_state_with_snakes(Op op) {
  return lens(mutate<&GameState::direction_command_filter_state>,
              read<&GameState::snakes>,
              std::move(op));
}

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
 * @tparam Op Function type: (food, scores, snakes) -> (food, scores)
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState
 */
template <typename Op>
auto over_food_and_scores_with_snakes(Op op) {
  return lens(mutate<&GameState::food_items, &GameState::scores>,
              read<&GameState::snakes>,
              std::move(op));
}

/**
 * @brief Lens decorator: Update both food and scores with access to snakes and board
 *
 * @tparam Op Function type: (food, scores, snakes, board) -> (food, scores)
 * @param op Operation to apply
 * @return State transformer: GameState -> GameState
 */
template <typename Op>
auto over_food_and_scores_with_snakes_and_board(Op op) {
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
