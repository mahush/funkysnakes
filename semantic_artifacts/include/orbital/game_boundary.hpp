#pragma once

#include <chrono>
#include <map>
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
 * messages. Every realization offers the same interactions. The boundary owns no rules: which
 * interaction a game admits in which phase is the Admission Policy's decision, and what an admitted
 * interaction does is decided by the Authorities it reaches.
 *
 * Its only counterparts are the players, so it has one end with an interface per direction:
 * PlayersEndInbound, implemented by the game, and PlayersEndOutbound, implemented by the players'
 * side. Time passing and randomness are no interactions: no counterpart controls them, so they
 * reach the game as execution inputs from whoever runs it. What they cause, such as steps, the
 * conclusion and the views, still goes out through PlayersEndOutbound.
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
// Domain System Boundary - players end, outbound
// ============================================================================

/**
 * @brief Why a start was refused
 */
enum class StartRejectionReason {
  ALREADY_STARTED  // A game has already started; a new game needs a new Domain Application
};

/**
 * @brief A start was refused; the game that already started is unchanged
 */
struct StartRejected {
  StartRejectionReason reason;
};

/**
 * @brief A submission was committed as an order
 */
struct SubmissionAccepted {
  OrderId order;
  GameTime committed_at;
};

/**
 * @brief Why a submission was refused
 */
enum class SubmissionRejectionReason {
  NOT_STARTED,  // No game has started yet
  ROUND_OVER    // The game has concluded
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
 * @brief Current views of the three Authorities, the game time and the orbital link condition
 *
 * The link condition is what the Order Release Policy decides: the Order Delay of a submission
 * issued now, the range of that delay, and the release time of each pending order. A combined
 * presentation is a projection of these views; it owns no meaning of its own and does not ask the
 * Policy itself.
 */
struct Views {
  arena_authority::View arena;
  order_authority::View orders;
  game_authority::View game;
  GameTime now;                                // Current time of the round's logical clock
  std::chrono::milliseconds submission_delay;  // Order Delay of a submission issued now (Order Release Policy)
  std::chrono::milliseconds min_delay;         // Lowest Order Delay (Order Release Policy)
  std::chrono::milliseconds max_delay;         // Highest Order Delay (Order Release Policy)
  std::map<PlayerId, std::vector<Direction>> pending_routes;  // Route each pending order would get if it activated now
  std::map<PlayerId, GameTime> pending_release_times;  // Release time of each pending order (Order Release Policy)
};

// ============================================================================
// Domain System Boundary - players end, interfaces
// ============================================================================

/**
 * @brief The players' end, inbound: what the game offers the players
 *
 * Implemented by the game. Whether an interaction is admitted is the Admission Policy's answer; a
 * refused one is reported through PlayersEndOutbound like any other outcome.
 */
class PlayersEndInbound {
 public:
  virtual ~PlayersEndInbound() = default;

  /**
   * @brief Begin the game
   *
   * @param start The start interaction
   */
  virtual void start(const Start& start) = 0;

  /**
   * @brief Submit an order
   *
   * @param submit The submission
   */
  virtual void submit(const Submit& submit) = 0;
};

/**
 * @brief The players' end, outbound: what the players learn from the game
 *
 * Implemented by the players' side. For each interaction or execution input, the game calls these
 * in order of occurrence: the start or submission outcome, the commitment, every step that ran
 * (oldest first), the orders that ended with the round, the conclusion, and finally the views.
 */
class PlayersEndOutbound {
 public:
  virtual ~PlayersEndOutbound() = default;

  /**
   * @brief A start was refused
   *
   * @param rejected Why
   */
  virtual void startRejected(const StartRejected& rejected) = 0;

  /**
   * @brief Outcome of a submission: committed as an order, or refused
   *
   * @param outcome The outcome
   */
  virtual void submissionOutcome(const SubmissionOutcome& outcome) = 0;

  /**
   * @brief The Order Authority's commitment record of an accepted submission
   *
   * @param committed The commitment record
   */
  virtual void orderCommitted(const OrderCommitted& committed) = 0;

  /**
   * @brief One arena step ran
   *
   * @param step What happened in it
   */
  virtual void stepReported(const StepReport& step) = 0;

  /**
   * @brief Executing orders ended because the game concluded; called only when there were any
   *
   * @param events Their endings
   */
  virtual void ordersEndedWithRound(const ArenaEvents& events) = 0;

  /**
   * @brief The game concluded
   *
   * @param concluded Its result
   */
  virtual void concluded(const game_authority::GameConcluded& concluded) = 0;

  /**
   * @brief Views after an interaction or execution input, once the game started
   *
   * @param views The current views
   */
  virtual void viewsUpdated(const Views& views) = 0;
};

}  // namespace game_boundary
}  // namespace orbital
}  // namespace snake
