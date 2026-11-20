#pragma once

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "common/game_types.hpp"
#include "common/players.hpp"
#include "orbital/arena_events.hpp"
#include "orbital/identities.hpp"
#include "orbital/order.hpp"
#include "orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {
namespace game_authority {

/**
 * Orbital Game Authority - owns the evolution of an Orbital Orders game as a whole
 *
 * Owns whether the game runs, the banked and held points, each player's handover chain and
 * the final result. A game is one round. It sits beside the Arena and Order Authorities: it reads
 * what the Arena Authority reports and the Order Authority's transmission history, uses the Orbital Scoring
 * Policy for point values, the Orbital Pacing Policy for the game duration and the Transmission Schedule
 * Policy for the handover window, and announces the
 * conclusion as an intent, which the realization routes to the Arena Authority (end of execution) and to the
 * Order Authority (cancel the orders not yet executing).
 *
 * Points: an event's points are applied as the Scoring Policy's point effects say; points held for
 * an order are settled when the order ends, as the Scoring Policy decides for its end reason. An
 * order ends when the Arena Authority reports its ending, or with the round when the game concludes while it
 * still executes.
 *
 * Within one step, the step's events are scored before conclusion is checked, so a step due at
 * the end of the game still counts.
 *
 * Handover chain: a successor H hands over from its predecessor F, the player's order that executed
 * last before H activated, when
 * - F completed (H activated without interrupting it; an idle gap between them is fine),
 * - H was submitted at or after the step that activated F and transmitted at the first opportunity at
 *   or after the step that completed F (Transmission Schedule Policy): so H was chosen in the interval
 *   around F's arrival, from the opportunity before it to the one after it. An order transmitted at an
 *   opportunity on a step's time activates only at a later step (Order Authority), so after F's
 *   completion in that step, and
 * - H replaced no earlier selection before it was transmitted.
 * Whether H hands over is decided when H activates: the handover then counts in the chain, is reported
 * as HandoverGranted and earns the
 * Scoring Policy's bonus for its position in it, held for H like its food points: banked when H completes,
 * dropped when H ends otherwise, which breaks the chain. An order activating without handing over, an
 * interruption, the player's snake dying (whether or not an order was executing) and the end of the game
 * break the chain; a rejected order never executed and changes nothing.
 * F's held points are settled by F's own ending and are not affected by its successor.
 *
 * The chain is derived from both histories as far as they are known. Transmissions may be learned
 * after the steps they relate to, as when a player-local Order Authority synchronizes late; the
 * chain is then derived again and the difference of the bonuses is banked when the transmission is
 * learned, so the eventual score is the same as if every transmission had been known in time.
 * Scores shown before the histories are complete are provisional. The Arena Authority's history
 * is never rewritten.
 */

/**
 * @brief Outcome of a concluded game
 */
struct Result {
  std::optional<PlayerId> winner;  // Highest banked score; empty for a draw

  bool operator==(const Result& other) const noexcept { return winner == other.winner; }
};

/**
 * @brief Intent announcing that the game concluded
 *
 * Conclusion ends the game's time: no step follows the one that concluded the game.
 */
struct GameConcluded {
  RoundId round;
  Result result;
};

/**
 * @brief An observed arena step as far as the handover chain needs it
 */
struct ObservedStep {
  StepId at;
  std::vector<ActivationOutcome> activations;
  ArenaEvents events;
  std::set<PlayerId> alive;  // Players whose snake survived the step
};

/**
 * @brief A handover the Game Authority counted
 */
struct HandoverGranted {
  PlayerId player;
  OrderNumber order;  // The successor that took over
  int chain;          // Its position in the chain
  int bonus;          // Bonus held for it until it ends

  bool operator==(const HandoverGranted& other) const noexcept {
    return player == other.player && order == other.order && chain == other.chain && bonus == other.bonus;
  }
};

/**
 * @brief A player's handover chain as derived from the known histories
 */
struct Chain {
  int length{0};        // Consecutive handovers of the current chain
  int bonus_banked{0};  // Handover bonus banked so far over the whole game
  int bonus_held{0};    // Handover bonus held for the executing order, banked if it completes
  // Completion of the player's last executed order while no other order activated since and the snake
  // lives; a successor hands over from it only if transmitted at the first opportunity at or after it
  std::optional<StepId> open_completion;
  std::optional<StepId> last_activation;             // Activation of the player's last executed order
  std::map<OrderNumber, HandoverGranted> handovers;  // Every handover counted over the game, by successor
};

/**
 * @brief Banked points per player and points held per executing order
 */
struct Scores {
  PerPlayerScores banked;
  std::map<PlayerId, std::map<OrderNumber, int>> held;  // Points held per player and executing order
};

struct View;
enum class HandoverProspect;
struct ProspectContext;

/**
 * @brief Game state owned by the Orbital Game Authority
 *
 * A value that callers store, pass and replace with the results of the transitions below. Its
 * fields are private: the game is read through the queries and evolves only through the
 * transitions.
 */
class State {
 private:
  RoundId round_;
  StepId now_{0};                 // Last observed step
  Scores scores_;                 // Banked points per player, held points per order
  std::optional<Result> result_;  // Set once the game concluded

  // Histories the handover chain is derived from
  std::map<PlayerId, std::vector<OrderTransmitted>> transmissions_;  // Learned transmissions per player
  std::vector<ObservedStep> steps_;                                  // Observed steps, in step order
  std::map<PlayerId, Chain> chains_;                                 // Derived chain and bonus banked so far

  // Construction
  friend State startRound(RoundId round, const std::vector<PlayerId>& players);

  // Transitions
  friend std::tuple<State, std::vector<HandoverGranted>, std::optional<GameConcluded>> observeStep(
      State state,
      StepId now,
      const std::vector<ActivationOutcome>& activations,
      const ArenaEvents& events,
      const std::set<PlayerId>& alive);
  friend std::tuple<State, std::vector<HandoverGranted>> observeTransmission(State state,
                                                                             const OrderTransmitted& transmitted);

  // Queries
  friend bool isRunning(const State& state);
  friend View view(const State& state);
  friend HandoverProspect handoverProspect(const State& state, const PlayerId& player, const ProspectContext& context);
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Start a game
 *
 * @param round Identity of the round
 * @param players Players taking part, all starting with zero points
 * @return Running game
 */
State startRound(RoundId round, const std::vector<PlayerId>& players);

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

/**
 * @brief Score one arena step and conclude the game if it is over
 *
 * The game is over with the Pacing Policy's last step or when no snake is alive any more.
 * Steps observed after conclusion change nothing.
 *
 * Activations of the step count as handovers as far as the activating orders' transmissions are
 * known; see observeTransmission for transmissions learned later.
 *
 * Admitted input: every step of this round exactly once, in step order, with its events in the
 * order the Arena Authority reported them. A step observed twice is scored twice, and steps out of order
 * can attribute food to an order that already ended; delivering complete steps once and in order
 * is the realization's guarantee.
 *
 * @param state Current game state
 * @param now The observed step
 * @param activations Activation outcomes of the step
 * @param events Arena events of the step, in order
 * @param alive Players whose snake is still alive after the step
 * @return Tuple of (updated state, handovers counted with this step, conclusion intent if the game
 *         concluded with this step)
 */
std::tuple<State, std::vector<HandoverGranted>, std::optional<GameConcluded>> observeStep(
    State state,
    StepId now,
    const std::vector<ActivationOutcome>& activations,
    const ArenaEvents& events,
    const std::set<PlayerId>& alive);

/**
 * @brief Learn that a player's order was transmitted
 *
 * The transmission counts with its own submission step, whenever it is learned. If steps already
 * observed show that the order took over as a handover, the handover is counted now and its bonus is
 * held or, if the order already completed, banked; if it changes which
 * order preceded a counted handover, that bonus is withdrawn. Transmissions learned after conclusion
 * or of another round change nothing. An order's transmission counts once: a later record for an
 * already known order, also with other steps, changes nothing, so a submission step cannot be
 * rewritten by resending.
 *
 * Admitted input: the Order Authority's transmissions of this round, in any order relative to the
 * steps.
 *
 * @param state Current game state
 * @param transmitted The transmission
 * @return Tuple of (updated game state, handovers counted now)
 */
std::tuple<State, std::vector<HandoverGranted>> observeTransmission(State state, const OrderTransmitted& transmitted);

// ============================================================================
// Queries: read the state without evolving it
// ============================================================================

/**
 * @brief Whether the game is still running (not yet concluded)
 *
 * @param state Current game state
 * @return True until the game concluded
 */
bool isRunning(const State& state);

/**
 * @brief A player's points
 */
struct PlayerScore {
  int banked;
  int held;           // Points of the executing order that count only if it completes
  int chain;          // Consecutive handovers of the current chain
  int next_handover;  // Bonus the next handover of the chain would earn
};

/**
 * @brief Projection of the game state for presentation
 */
struct View {
  std::map<PlayerId, PlayerScore> players;
  int remaining_steps;  // Steps still to come up to the last step of the game
  std::optional<Result> result;
};

/**
 * @brief Project the game state
 *
 * @param state Current game state
 * @return Points per player, remaining steps and the result once concluded
 */
View view(const State& state);

/**
 * @brief What choosing a successor would earn a player now
 */
enum class HandoverProspect {
  NONE,            // No handover window is open, or nothing chosen now could hand over
  CHOOSE_NOW,      // A successor chosen now would hand over
  SUCCESSOR_READY  // The selected successor hands over if it takes over as expected
};

/**
 * @brief What the other owners know about a player's orders, as the handover prospect needs it
 */
struct ProspectContext {
  StepId now;                                 // Current step, not before the last observed step
  std::optional<StepId> expected_completion;  // Step at which the executing order is expected to complete
  std::optional<StepId> selected_at;          // Submission step of the selected, not yet transmitted order
  bool selection_replaced{false};             // Whether that selection replaced an earlier one
  bool transmission_pending{false};           // Whether a transmitted order waits for activation
};

/**
 * @brief What choosing a successor would earn a player now
 *
 * The predecessor is the executing order, completing as expected, or else the last completed order
 * while its completion is open. A choice made now hands over if the opportunity of its submission is the
 * first opportunity after that completion (both Transmission Schedule Policy): the window. The
 * game concludes at the end of its duration before anything is transmitted then, so a window whose
 * opportunity falls at or after that end offers nothing. A transmitted order waiting for activation takes over first,
 * so nothing chosen now hands over from the predecessor. A selection hands over only if it replaced none and was
 * submitted at or after the predecessor's activation, as for a counted handover.
 *
 * @param state Current game state
 * @param player The player
 * @param context The player's orders and current step as the other owners know them
 * @return NONE after conclusion, outside the window and when nothing chosen now could hand over;
 *         CHOOSE_NOW in the window without a selection; SUCCESSOR_READY for a qualifying selection
 */
HandoverProspect handoverProspect(const State& state, const PlayerId& player, const ProspectContext& context);

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
