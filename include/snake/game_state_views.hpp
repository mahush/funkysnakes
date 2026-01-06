#ifndef SNAKE_GAME_STATE_VIEWS_HPP
#define SNAKE_GAME_STATE_VIEWS_HPP

#include "snake/game_messages.hpp"
#include "snake/generic_view.hpp"

namespace snake {

// Forward declarations
struct GameState;

// ============================================================================
// View Decorators - Read-only field extractors
// ============================================================================
// View decorators extract fields from state for read-only access without
// mutation. They follow the `with_*` naming pattern (vs `over_*` for lenses).
// - Return a function that takes state and additional args
// - Pass extracted fields to operation
// - Return operation result directly (not wrapped in state)
//
// Usage pattern:
//   auto extractor = with_board_and_snakes(generateFood);  // Create extractor
//   Point pos = extractor(state, random_int);               // Apply to state

/**
 * @brief View decorator: Extract board and snakes for read-only access
 *
 * Returns a decorator that extracts board and snakes from GameState and passes
 * them to the operation along with any additional arguments. Does not mutate state.
 *
 * Example usage:
 *   auto extractor = with_board_and_snakes(generateRandomFoodPosition);
 *   Point pos = extractor(state, random_int);
 *
 * @tparam Op Function type: (Board, snakes, args...) -> Result
 * @param op Operation to apply
 * @return View transformer: (GameState, args...) -> Result
 */
template <typename Op>
auto with_board_and_snakes(Op op) {
  return view(read<&GameState::board, &GameState::snakes>, std::move(op));
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_VIEWS_HPP
