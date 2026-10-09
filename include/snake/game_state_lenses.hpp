#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <utility>

#include "snake/game_messages.hpp"
#include "snake/generic_lens.hpp"

namespace snake {

// ============================================================================
// Lens Decorators - Built on generic lens for consistent behavior
// ============================================================================
// Lenses are state transformers that follow the `over_*` naming pattern.
// Lenses over the arena's parts are internal to the Classic Arena Authority;
// the actor only focuses whole arena transitions on its arena field.
//
// Usage pattern:
//   auto transformer = over_arena(classic_arena_authority::steer);  // Create transformer
//   new_state = transformer(state, cmd);                             // Apply to state

/**
 * @brief Lens decorator: Update the arena
 *
 * Returns a state transformer that applies a Classic Arena Authority transition
 * to the arena held in GameState.
 *
 * @tparam TOp Function type: (arena, args...) -> arena
 * @param op Operation to apply
 * @return State transformer: (GameState, args...) -> GameState
 */
template <typename TOp>
auto over_arena(TOp op) {
  return lens(mutate<&GameState::arena>, read<>, std::move(op));
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
