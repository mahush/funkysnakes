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
 * @brief Lens decorator: Consume directions from command queues
 *
 * Returns a state transformer that applies a consume operation to direction command state
 * and returns both the updated state and the consumed directions as a tuple.
 *
 * Example usage:
 *   auto transformer = over_direction_command_consuming(direction_command_filter::try_consume_next);
 *   auto [new_state, consumed] = transformer(state);
 *
 * @tparam Op Function type: (command_state) -> ConsumeResult
 * @param op Consume operation to apply
 * @return State transformer: GameState -> tuple<GameState, ConsumedDirections>
 */
template <typename Op>
auto over_direction_command_consuming(Op op) {
  return [op = std::move(op)](GameState state) {
    auto result = op(state.direction_command);
    state.direction_command = result.filters;
    return std::make_tuple(std::move(state), std::move(result.consumed_directions));
  };
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
// Lens Decorators - Return state transformers for functional composition
// ============================================================================
// Each lens takes an operation (and optional args) and returns a function
// that transforms GameState. This enables clean pipeline composition without
// wrapping lambdas: pipe(state, over_snakes(op1), over_food(op2), ...)

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
  return [op = std::move(op)](GameState state, auto&&... args) {
    state.snakes = op(std::move(state.snakes), std::forward<decltype(args)>(args)...);
    return state;
  };
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
  return [op = std::move(op)](GameState state) {
    state.snakes = op(std::move(state.snakes), state.board, state.food_items);
    return state;
  };
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
  return [op = std::move(op)](GameState state) {
    auto result = op(std::move(state.snakes), std::move(state.scores));

    // Check tuple size to handle both cases
    if constexpr (std::tuple_size_v<decltype(result)> == 2) {
      // Simple case: (snakes, scores) → GameState
      auto& [new_snakes, new_scores] = result;
      state.snakes = std::move(new_snakes);
      state.scores = std::move(new_scores);
      return state;
    } else {
      // Extended case: (snakes, scores, additional_result) → (GameState, additional_result)
      auto& [new_snakes, new_scores, additional_result] = result;
      state.snakes = std::move(new_snakes);
      state.scores = std::move(new_scores);
      return std::make_tuple(std::move(state), std::move(additional_result));
    }
  };
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
  return [op = std::move(op)](GameState state) {
    auto [new_food, new_scores] = op(std::move(state.food_items), std::move(state.scores),
                                      state.snakes, state.board);
    state.food_items = std::move(new_food);
    state.scores = std::move(new_scores);
    return state;
  };
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
  return [op = std::move(op)](GameState state, auto&&... args) {
    state.food_items = op(std::move(state.food_items), std::forward<decltype(args)>(args)...);
    return state;
  };
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
  return [op = std::move(op)](GameState state) {
    state.food_items = op(std::move(state.food_items), state.snakes);
    return state;
  };
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
  return [op = std::move(op)](GameState state) mutable {
    state.food_items = op(std::move(state.food_items), state.board, state.snakes);
    return state;
  };
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
