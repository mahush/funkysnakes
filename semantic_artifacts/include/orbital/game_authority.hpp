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
 * Owns whether the game runs, the credited and held points, each player's handover chain and
 * the final result. A game is one round. It sits beside the Arena and Order Authorities: it reads
 * what the Arena reports and the Order Authority's commitment history, uses the Orbital Scoring
 * Policy for point values and the Orbital Pacing Policy for the game duration, and announces the
 * conclusion as an intent, which the realization routes to the Arena (end of execution) and to the
 * Order Authority (cancel pending orders).
 *
 * Points: an event's points are applied as the Scoring Policy's point effects say; points held for
 * an order are settled when the order ends, as the Scoring Policy decides for its end reason. An
 * order ends when the Arena reports its ending, or with the round when the game concludes while it
 * still executes.
 *
 * Within one step, the step's events are scored before conclusion is checked, so a step due at
 * the end of the game still counts.
 *
 * Handover chain: an order completing while its prepared successor is still pending is a handover;
 * it earns the Scoring Policy's bonus for its position in the chain. The prepared successor is the
 * player's one commitment made while the order executed: from the step that activated it and before
 * the completing step. It is still pending if it has not been activated or rejected by then. A
 * second commitment in that time revises the prepared successor and costs the executing order its
 * handover; later commitments cannot restore it, while the executing order's held points
 * are unaffected. The chain breaks with a completion that is no handover, an interruption, the
 * player's snake dying (whether or not an order was executing), a rejection of the successor or the
 * end of the game.
 *
 * The chain is derived from both histories as far as they are known. Commitments may be learned
 * after the steps they relate to, as when a player-local Order Authority synchronizes late; the
 * chain is then derived again and the difference of the bonuses is credited when the commitment is
 * learned. Usually that adds a bonus; it withdraws one that was credited on incomplete history,
 * when a later commitment shows that the counted successor had already been revised. Either
 * way, the eventual score is the same as if every commitment had been known in time, so scores
 * shown before the histories are complete are provisional. Arena history is never rewritten.
 */

/**
 * @brief Outcome of a concluded game
 */
struct Result {
  std::optional<PlayerId> winner;  // Highest credited score; empty for a draw

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
  int length{0};          // Consecutive handovers of the current chain
  int bonus_credited{0};  // Handover bonus credited so far over the whole game
};

/**
 * @brief Points held for one executing order
 */
struct Held {
  PlayerId player;
  int points;
};

/**
 * @brief Credited points per player and points held per executing order
 */
struct Scores {
  PerPlayerScores credited;
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
  Scores scores_;                 // Credited points per player, held points per order
  std::optional<Result> result_;  // Set once the game concluded

  // Histories the handover chain is derived from
  std::map<PlayerId, std::vector<OrderCommitted>> commitments_;  // Learned commitments per player
  std::vector<ObservedStep> steps_;                              // Observed steps, in step order
  std::map<PlayerId, Chain> chains_;                             // Derived chain and bonus credited so far

  // Construction
  friend State startRound(RoundId round, const std::vector<PlayerId>& players);

  // Transitions
  friend std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                                     GameTime now,
                                                                     const std::vector<ActivationOutcome>& activations,
                                                                     const ArenaEvents& events,
                                                                     const std::set<PlayerId>& alive);
  friend State observeCommitment(State state, const OrderCommitted& committed);

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
 * Completions of the step earn handover bonuses as far as the successors' commitments are known;
 * see observeCommitment for commitments learned later.
 *
 * Admitted input: every step of this round exactly once, in step order, with its events in the
 * order the Arena reported them. A step observed twice is scored twice, and steps out of order
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
 * @brief Learn that a player committed an order
 *
 * The commitment counts with its own commitment time, whenever it is learned. If steps already
 * observed show that the order was a successor pending at a completion, the handover bonus earned
 * there is credited now; if it shows that a counted successor had been revised, that bonus is
 * withdrawn. Commitments learned after conclusion or of another round change nothing. An order's
 * commitment counts once: a later record for an already known order, also with another time,
 * changes nothing, so a commitment time cannot be rewritten by resending.
 *
 * Admitted input: the Order Authority's commitments of this round, in any order relative to the
 * steps.
 *
 * @param state Current game state
 * @param committed The commitment
 * @return Updated game state
 */
State observeCommitment(State state, const OrderCommitted& committed);

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
  int credited;
  int held;           // Points of the executing order that count only if it completes
  int chain;          // Consecutive handovers of the current chain
  int next_handover;  // Bonus the next handover of the chain would earn
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
