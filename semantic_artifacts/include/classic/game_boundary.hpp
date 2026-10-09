#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "classic/game_types.hpp"
#include "classic/players.hpp"

namespace snake {
namespace game_boundary {

/**
 * Domain System Boundary of classic snake
 *
 * The game's external interactions, expressed as intents and observations rather than devices or
 * messages. Every realization (the Domain Application, the actor-based production game) offers the
 * same interactions; production messages carry these types as their payload.
 */

// ============================================================================
// Domain System Boundary - players end, inbound
// ============================================================================

/**
 * @brief Begin a new game
 */
struct Start {
  int starting_level{1};
};

/**
 * @brief A player's intended turn
 */
struct Steer {
  PlayerId player;
  Direction direction;
};

/**
 * @brief Pause or resume the game
 */
struct TogglePause {};

// ============================================================================
// Domain System Boundary - external facts, inbound
// ============================================================================

/**
 * @brief Game time has passed
 */
struct TimeElapsed {
  std::chrono::milliseconds duration;
};

// ============================================================================
// Domain System Boundary - players end, outbound
// ============================================================================

/**
 * @brief Board, snakes, food and scores after an arena step
 */
struct ArenaView {
  Board board;
  PerPlayerSnakes snakes;
  FoodItems food_items;
  PerPlayerScores scores;
};

/**
 * @brief Current level and whether the game is paused
 */
struct Status {
  int level;
  bool paused;
};

/**
 * @brief Final result of a concluded game
 */
struct GameOver {
  PerPlayerScores final_scores;
  int final_level;
};

/**
 * @brief Everything observable at the boundary after one interaction, in order of occurrence
 */
struct Observations {
  std::vector<ArenaView> arena_views;
  std::vector<Status> statuses;
  std::optional<GameOver> game_over;
};

}  // namespace game_boundary
}  // namespace snake
