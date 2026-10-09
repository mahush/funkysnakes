#pragma once

#include <chrono>
#include <optional>
#include <variant>
#include <vector>

#include "common/game_primitives.hpp"
#include "common/game_time.hpp"
#include "common/players.hpp"
#include "orbital/arena_authority.hpp"
#include "orbital/arena_events.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/identities.hpp"
#include "orbital/order.hpp"
#include "orbital/order_authority.hpp"

namespace snake {
namespace orbital {
namespace game_boundary {

/**
 * Domain System Boundary of Orbital Orders
 *
 * The game's external interactions, expressed as intents and observations rather than devices or
 * messages. Every realization offers the same interactions.
 *
 * Submission rule of the boundary: a submission is accepted while the game is running (as the
 * Game Authority says); afterwards it is rejected with ROUND_OVER. The issue time of a submission
 * is the current time of the round's logical clock; submissions are never backdated.
 */

// ============================================================================
// Domain System Boundary - players end, inbound
// ============================================================================

/**
 * @brief Begin a new game (one round)
 */
struct Start {
  RoundId round;
};

/**
 * @brief A player orders their snake to a board position
 */
struct Submit {
  PlayerId player;
  Point target;
};

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
 * @brief A submission became an order
 */
struct SubmissionAccepted {
  OrderId order;
  GameTime issued_at;
};

/**
 * @brief Why a submission was refused
 */
enum class SubmissionRejectionReason {
  ROUND_OVER  // The game is not running (not started or already concluded)
};

/**
 * @brief A submission was refused
 */
struct SubmissionRejected {
  SubmissionRejectionReason reason;
};

using SubmissionOutcome = std::variant<SubmissionAccepted, SubmissionRejected>;

/**
 * @brief What happened in one arena step
 */
struct StepReport {
  StepId step;
  GameTime at;
  std::vector<ActivationOutcome> activations;
  ArenaEvents events;
};

/**
 * @brief Current views of the three Authorities
 *
 * A combined presentation is a projection of these views; it owns no meaning of its own.
 */
struct Views {
  arena_authority::View arena;
  order_authority::View orders;
  game_authority::View game;
};

/**
 * @brief Everything observable at the boundary after one interaction, in order of occurrence
 */
struct Observations {
  std::optional<SubmissionOutcome> submission;  // For Submit
  std::vector<StepReport> steps;                // Every step that ran, oldest first
  ArenaEvents end_of_round_events;              // Orders ended because the game concluded
  std::optional<game_authority::GameConcluded> concluded;
  std::optional<Views> views;  // Views after the interaction, once started
};

}  // namespace game_boundary
}  // namespace orbital
}  // namespace snake
