#ifndef SNAKE_SNAKE_MODEL_HPP
#define SNAKE_SNAKE_MODEL_HPP

#include <array>
#include <functional>
#include <utility>
#include <vector>

#include "snake/game_messages.hpp"
#include "snake/state_with_effect.hpp"

namespace snake {

/**
 * @brief Pair of snakes for collision detection
 *
 * Used for functional collision handling between two snakes.
 */
struct SnakePair {
  Snake a;
  Snake b;
};

/**
 * @brief Result of cutting a snake's tail
 *
 * Contains both the truncated snake and the segments that were cut off.
 */
struct CutResult {
  Snake snake;                      // Truncated snake
  std::vector<Point> cut_segments;  // Segments that were removed
};

/**
 * @brief Get the head position of a snake
 *
 * Always returns the head position since Snake always has a head.
 *
 * @param snake Snake
 * @return Head position
 */
Point getHead(const Snake& snake);

/**
 * @brief Cut tail of snake starting at a specific point
 *
 * Returns both the truncated snake and the segments that were cut off.
 * The cut point and everything after it are removed from the snake and
 * returned as cut_segments. If the cut point is the head or not found,
 * returns the original snake with empty cut_segments.
 *
 * @param snake Original snake
 * @param hitPoint Point where to start cutting tail (excluded from result)
 * @return CutResult containing truncated snake and cut segments
 */
CutResult cutTailAt(const Snake& snake, const Point& hitPoint);

/**
 * @brief Check if first snake's head bites second snake's body
 *
 * Returns true if the first snake's head is contained anywhere in the
 * second snake's body (head or tail).
 *
 * @param first Snake that might be biting
 * @param second Snake that might be bitten
 * @return true if first bites second
 */
bool firstBitesSecond(const Snake& first, const Snake& second);

/**
 * @brief Check if both snakes bite each other simultaneously
 *
 * Returns true if both snakes' heads hit each other's bodies at the same time.
 *
 * @param a First snake
 * @param b Second snake
 * @return true if both bite each other
 */
bool bothBiteEachOther(const Snake& a, const Snake& b);

/**
 * @brief Apply bite rule to two snakes
 *
 * Pure functional implementation of collision detection:
 * - If snake A's head hits snake B's body, cut B at the hit point
 * - If snake B's head hits snake A's body, cut A at the hit point
 * - Otherwise, return snakes unchanged
 *
 * @param s Pair of snakes
 * @return New pair of snakes with bite rule applied
 */
SnakePair applyBiteRule(SnakePair s);

// ============================================================================
// Snake Lens Functions - Focus on snakes within game state
// ============================================================================

/**
 * @brief Apply a function to each snake in the game state
 *
 * This traversal maps an effectful transformation over all snakes,
 * calling the function once per snake and combining all effects.
 *
 * @tparam F Function type: Snake -> SnakeWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param f Function to apply to each snake
 * @return Updated game state with combined effects from all snakes
 */
template <typename F>
GameStateWithEffect over_each_snake_combining_scores(GameStateWithEffect x, F f) {
  for (auto& [player_id, snake] : x.state.snakes) {
    auto res = f(snake);
    snake = res.state;
    // Lift score delta into game effect
    x.effect = combine(x.effect, GameEffect::withScores(res.effect));
  }
  return x;
}

/**
 * @brief Apply a function to each alive snake
 *
 * This traversal filters to alive snakes before applying the transformation,
 * calling the function once per alive snake and combining all effects.
 *
 * @tparam F Function type: Snake -> SnakeWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param f Function to apply to each alive snake
 * @return Updated game state with combined effects from all alive snakes
 */
template <typename F>
GameStateWithEffect over_each_alive_snake_combining_scores(GameStateWithEffect x, F f) {
  for (auto& [player_id, snake] : x.state.snakes) {
    if (snake.alive) {
      auto res = f(snake);
      snake = res.state;
      // Lift score delta into game effect
      x.effect = combine(x.effect, GameEffect::withScores(res.effect));
    }
  }
  return x;
}

// Helper to unpack tuple results and update state (C++17 compatible)
template <typename ResultTuple, std::size_t... Is>
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
 * creates std::pair<PlayerId, Snake> for each, passes them all to the function
 * in a single call, and unpacks the results.
 *
 * Use cases:
 * - Single snake (self-collision): over_selected_snakes_combining_scores(x, f, "player1")
 *   → calls f(pair1) once
 * - Two snakes (collision): over_selected_snakes_combining_scores(x, f, "player1", "player2")
 *   → calls f(pair1, pair2) once
 *
 * @tparam F Function type taking N pairs and returning tuple of (N Snakes..., GameEffect)
 * @param x Current game state with accumulated effects
 * @param f Function to apply over the selected snakes (called once with all snakes)
 * @param player_ids Variable number of player IDs to operate on
 * @return Updated game state with combined effects
 */
template <typename F, typename... PlayerIds>
GameStateWithEffect over_selected_snakes_combining_scores(GameStateWithEffect x, F f,
                                                          const PlayerIds&... player_ids) {
  // Helper to look up snakes and create pairs
  auto lookup = [&](const PlayerId& pid) -> std::pair<PlayerId, Snake> {
    auto it = x.state.snakes.find(pid);
    if (it != x.state.snakes.end()) {
      return {pid, it->second};
    }
    // Return empty snake if not found (caller should handle)
    return {pid, Snake{}};
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

}  // namespace snake

#endif  // SNAKE_SNAKE_MODEL_HPP
