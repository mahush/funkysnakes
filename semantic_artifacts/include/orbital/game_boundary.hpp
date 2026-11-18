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
 * @brief A submission became the player's selected order
 */
struct SubmissionAccepted {
  OrderId order;
  StepId submitted_at;
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
  std::vector<ActivationOutcome> activations;
  ArenaEvents events;
  std::vector<OrderTransmitted> transmissions;             // Orders transmitted at the end of the step
  std::vector<game_authority::HandoverGranted> handovers;  // Handovers the Game Authority counted with the step
};

/**
 * @brief Current views of the three Authorities, the game time and the orbital link condition
 *
 * The link condition is what the Transmission Schedule Policy decides: every transmission
 * opportunity of the game. The step interval is the Pacing Policy's. A combined presentation is a
 * projection of these views; it owns no meaning of its own and does not ask the Policies itself.
 */
struct Views {
  arena_authority::View arena;
  order_authority::View orders;
  game_authority::View game;
  GameTime now;                                                   // Current time of the round's logical clock
  std::chrono::milliseconds step_interval;                        // Interval between arena steps (Pacing Policy)
  std::vector<StepId> transmission_schedule;                      // Every opportunity of the game, in step order
  std::map<PlayerId, std::vector<Direction>> selected_routes;     // Route each selected order would get now
  std::map<PlayerId, std::vector<Direction>> transmitted_routes;  // Route each transmitted order would get now
  std::map<PlayerId, game_authority::HandoverProspect> handover_prospects;  // What choosing a successor earns now
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
 * in order of occurrence: the start or submission outcome, the submission record, every step that ran
 * (oldest first, each with the orders transmitted at its end), the orders that ended with the round, the
 * conclusion, and finally the views.
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
   * @brief Outcome of a submission: selected as an order, or refused
   *
   * @param outcome The outcome
   */
  virtual void submissionOutcome(const SubmissionOutcome& outcome) = 0;

  /**
   * @brief The Order Authority's submission record of an accepted submission
   *
   * @param submitted The submission record
   */
  virtual void orderSubmitted(const OrderSubmitted& submitted) = 0;

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
