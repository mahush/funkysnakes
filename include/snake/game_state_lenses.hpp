#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>
#include <vector>

#include "snake/state_with_effect.hpp"

namespace snake {

/**
 * @brief Apply a function to a specific snake by player ID
 *
 * This lens focuses on a single snake within the game state, applies
 * an effectful transformation, and properly combines the effects.
 *
 * @tparam F Function type: SnakeState -> SnakeStateWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param player_id ID of the player whose snake to transform
 * @param f Function to apply to the snake
 * @return Updated game state with combined effects
 */
template <typename F>
GameStateAndScoreDelta over_snake_with_effects(GameStateAndScoreDelta x, const PlayerId& player_id, F f) {
  auto it = x.state.snakes.find(player_id);
  if (it == x.state.snakes.end()) {
    return x;  // Player not found, return unchanged
  }

  auto res = f(it->second);
  it->second = res.state;
  x.effect = combine(x.effect, res.effect);
  return x;
}

/**
 * @brief Apply a function to each snake in the game state
 *
 * This iteration maps an effectful transformation over all snakes,
 * calling the function once per snake and combining all effects.
 *
 * @tparam F Function type: SnakeState -> SnakeStateWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param f Function to apply to each snake
 * @return Updated game state with combined effects from all snakes
 */
template <typename F>
GameStateAndScoreDelta for_each_snake_combining_scores(GameStateAndScoreDelta x, F f) {
  for (auto& [player_id, snake_state] : x.state.snakes) {
    auto res = f(snake_state);
    snake_state = res.state;
    x.effect = combine(x.effect, res.effect);
  }
  return x;
}

/**
 * @brief Apply a function to each alive snake
 *
 * This iteration filters to alive snakes before applying the transformation,
 * calling the function once per alive snake and combining all effects.
 *
 * @tparam F Function type: SnakeState -> SnakeStateWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param f Function to apply to each alive snake
 * @return Updated game state with combined effects from all alive snakes
 */
template <typename F>
GameStateAndScoreDelta for_each_alive_snake_combining_scores(GameStateAndScoreDelta x, F f) {
  for (auto& [player_id, snake_state] : x.state.snakes) {
    if (snake_state.alive) {
      auto res = f(snake_state);
      snake_state = res.state;
      x.effect = combine(x.effect, res.effect);
    }
  }
  return x;
}

// Helper to unpack tuple results and update state (C++17 compatible)
template <typename GameStateWithEffect, typename ResultTuple, std::size_t... Is>
void update_snakes_from_result(GameStateWithEffect& x, const ResultTuple& result,
                                const std::array<PlayerId, sizeof...(Is)>& players, std::index_sequence<Is...>) {
  ((x.state.snakes[players[Is]] = std::get<Is>(result)), ...);
  x.effect = combine(x.effect, std::get<sizeof...(Is)>(result));
}

/**
 * @brief Lens to apply a function over explicitly selected snakes
 *
 * This variadic lens focuses on explicitly selected snakes and calls the function
 * ONCE with all selected snakes as parameters. It looks up each player ID,
 * creates std::pair<PlayerId, SnakeState> for each, passes them all to the function
 * in a single call, and unpacks the results.
 *
 * Use cases:
 * - Single snake (self-collision): over_selected_snakes_combining_scores(x, f, "player1")
 *   → calls f(pair1) once
 * - Two snakes (collision): over_selected_snakes_combining_scores(x, f, "player1", "player2")
 *   → calls f(pair1, pair2) once
 *
 * @tparam F Function type taking N pairs and returning tuple of (N SnakeStates..., ScoreDelta)
 * @param x Current game state with accumulated effects
 * @param f Function to apply over the selected snakes (called once with all snakes)
 * @param player_ids Variable number of player IDs to operate on
 * @return Updated game state with combined effects
 */
template <typename F, typename... PlayerIds>
GameStateAndScoreDelta over_selected_snakes_combining_scores(GameStateAndScoreDelta x, F f,
                                                              const PlayerIds&... player_ids) {
  // Helper to look up snake states and create pairs
  auto lookup = [&](const PlayerId& pid) -> std::pair<PlayerId, SnakeState> {
    auto it = x.state.snakes.find(pid);
    if (it != x.state.snakes.end()) {
      return {pid, it->second};
    }
    // Return empty state if not found (caller should handle)
    return {pid, SnakeState{}};
  };

  // Check all players exist
  auto check_exists = [&](const PlayerId& pid) { return x.state.snakes.find(pid) != x.state.snakes.end(); };
  if (!(check_exists(player_ids) && ...)) {
    return x;  // At least one player not found
  }

  // Call function with pairs
  auto result = f(lookup(player_ids)...);

  // Unpack result and update state
  // Convert player_ids to array of PlayerId (std::string)
  std::array<PlayerId, sizeof...(PlayerIds)> players{PlayerId(player_ids)...};
  update_snakes_from_result(x, result, players, std::make_index_sequence<sizeof...(PlayerIds)>{});

  return x;
}

/**
 * @brief Lift a pure state transformation into the effectful context
 *
 * Helper to convert functions that only modify state (no effects)
 * into the StateWithEffect format with empty effects.
 *
 * @tparam State The state type
 * @tparam Effect The effect type
 * @tparam F Function type: State -> State
 * @param f Pure state transformation
 * @return Effectful transformation that produces empty effects
 */
template <typename State, typename Effect, typename F>
auto with_empty_effect(F f) {
  return [f](State s) -> StateWithEffect<State, Effect> { return {f(s), Effect::empty()}; };
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
