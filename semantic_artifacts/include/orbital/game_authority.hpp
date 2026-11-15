#pragma once

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "common/game_time.hpp"
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
 * Handover chain: a successor H that completes is a handover from its predecessor F, the
 * player's order that executed last before H activated, when
 * - F completed (H activated without interrupting it; an idle gap between them is fine),
 * - H was submitted at or after the step that activated F and transmitted at the first opportunity at
 *   or after the step that completed F (Transmission Schedule Policy): so H was chosen in the interval
 *   around F's arrival, from the opportunity before it to the one after it, and
 * - H replaced no earlier selection before it was transmitted.
 * Whether H hands over is known when H activates: the handover then counts in the chain and earns the
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
  GameTime at;
  std::vector<ActivationOutcome> activations;
  ArenaEvents events;
  std::set<PlayerId> alive;  // Players whose snake survived the step
};

/**
 * @brief A player's handover chain as derived from the known histories
 */
struct Chain {
  int length{0};        // Consecutive handovers of the current chain
  int bonus_banked{0};  // Handover bonus banked so far over the whole game
  int bonus_held{0};    // Handover bonus held for the executing order, banked if it completes
  // Completion of the player's last executed order while no other order executed since: a successor
  // may still hand over from it
  std::optional<GameTime> open_completion;
};

/**
 * @brief Points held for one executing order
 */
struct Held {
  PlayerId player;
  int points;
};

/**
 * @brief Banked points per player and points held per executing order
 */
struct Scores {
  PerPlayerScores banked;
  std::map<OrderId, Held> held;
};

struct View;

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
  GameTime now_{0};               // Time of the last observed step
  Scores scores_;                 // Banked points per player, held points per order
  std::optional<Result> result_;  // Set once the game concluded

  // Histories the handover chain is derived from
  std::map<PlayerId, std::vector<OrderTransmitted>> transmissions_;  // Learned transmissions per player
  std::vector<ObservedStep> steps_;                                  // Observed steps, in step order
  std::map<PlayerId, Chain> chains_;                                 // Derived chain and bonus banked so far

  // Construction
  friend State startRound(RoundId round, const std::vector<PlayerId>& players);

  // Transitions
  friend std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                                     GameTime now,
                                                                     const std::vector<ActivationOutcome>& activations,
                                                                     const ArenaEvents& events,
                                                                     const std::set<PlayerId>& alive);
  friend State observeTransmission(State state, const OrderTransmitted& transmitted);

  // Queries
  friend bool isRunning(const State& state);
  friend View view(const State& state);
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
 * The game is over when the Pacing Policy's game duration has elapsed or no snake is alive any more.
 * Steps observed after conclusion change nothing.
 *
 * Completions of the step earn handover bonuses as far as the completing orders' transmissions are
 * known; see observeTransmission for transmissions learned later.
 *
 * Admitted input: every step of this round exactly once, in step order, with its events in the
 * order the Arena Authority reported them. A step observed twice is scored twice, and steps out of order
 * can attribute food to an order that already ended; delivering complete steps once and in order
 * is the realization's guarantee.
 *
 * @param state Current game state
 * @param now Time of the step on the round's timeline
 * @param activations Activation outcomes of the step
 * @param events Arena events of the step, in order
 * @param alive Players whose snake is still alive after the step
 * @return Tuple of (updated state, conclusion intent if the game concluded with this step)
 */
std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                            GameTime now,
                                                            const std::vector<ActivationOutcome>& activations,
                                                            const ArenaEvents& events,
                                                            const std::set<PlayerId>& alive);

/**
 * @brief Learn that a player's order was transmitted
 *
 * The transmission counts with its own submission time, whenever it is learned. If steps already
 * observed show that the order completed as a handover, its bonus is banked now; if it changes which
 * order preceded a counted handover, that bonus is withdrawn. Transmissions learned after conclusion
 * or of another round change nothing. An order's transmission counts once: a later record for an
 * already known order, also with other times, changes nothing, so a submission time cannot be
 * rewritten by resending.
 *
 * Admitted input: the Order Authority's transmissions of this round, in any order relative to the
 * steps.
 *
 * @param state Current game state
 * @param transmitted The transmission
 * @return Updated game state
 */
State observeTransmission(State state, const OrderTransmitted& transmitted);

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
  // Time of the step that completed the player's last executed order, while no other order activated
  // since and the snake lives: a successor sent at the first opportunity from then on hands over
  std::optional<GameTime> open_completion;
};

/**
 * @brief Projection of the game state for presentation
 */
struct View {
  std::map<PlayerId, PlayerScore> players;
  GameTime remaining;
  std::optional<Result> result;
};

/**
 * @brief Project the game state
 *
 * @param state Current game state
 * @return Points per player, remaining time and the result once concluded
 */
View view(const State& state);

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
