#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "domain/orbital/order.hpp"

namespace snake {
namespace orbital {
namespace order_selection_authority {

/**
 * Order Selection Authority - owns what each player has chosen until it is sent
 *
 * Owns the numbering of the players' submissions and each player's selection: the latest submission, until it is
 * transmitted at its opportunity. It does not own when the orbital link offers an opportunity (Transmission Schedule
 * Policy), what happens to a transmitted order (Arena Authority) or any points (Game Authority).
 *
 * - An order is identified by its player and its number; each player's submissions are numbered from 1.
 * - A submission takes effect where the player's input is accepted: it becomes the player's selection at once, with
 *   its own submission step, which is part of the domain history and published as OrderSubmitted.
 * - A new submission replaces the player's selection; the replaced order is never transmitted.
 * - At a transmission, each selection whose opportunity has come is sent irrevocably and published as
 *   OrderTransmitted, keeping its own submission step. It stops being the player's selection; a correction is a new
 *   submission, transmitted later.
 */

/**
 * @brief A player's selected order, waiting to be transmitted
 */
struct Selection {
  Order order;
  StepId submitted_at;
  std::optional<OrderNumber> replaced;  // The selection this submission replaced

  bool operator==(const Selection& other) const {
    return order == other.order && submitted_at == other.submitted_at && replaced == other.replaced;
  }
};

struct Submission;
struct View;

/**
 * @brief Selection state owned by the Order Selection Authority
 *
 * A value that callers store, pass and replace with the results of the transitions below. Its fields are private:
 * selections are read through the queries and evolve only through the transitions.
 */
class State {
 private:
  RoundId round_;
  std::map<PlayerId, int> submitted_count_;   // Orders submitted so far per player
  std::map<PlayerId, Selection> selections_;  // The selected, not yet transmitted order per player

  // Construction
  friend State startRound(RoundId round);

  // Transitions
  friend std::tuple<State, OrderSubmitted> submit(State state, const Submission& submission);
  friend std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at);
  friend State cancel(State state);

  // Queries
  friend View view(const State& state);
};

/**
 * @brief A player's request to move their snake to a position
 */
struct Submission {
  PlayerId player;
  Point target;
  StepId submitted_at;  // Step in which the player submitted it
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Start the order selection of a round
 *
 * @param round Identity of the round
 * @return State without any selection
 */
State startRound(RoundId round);

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

/**
 * @brief Select a submission as the player's new order
 *
 * The order gets the player's next number and becomes the player's selection, replacing the player's previous
 * selection if there is one. Orders already transmitted are unaffected.
 *
 * @param state Current selection state
 * @param submission The player's request
 * @return Tuple of (updated state, submission record with the new order's number)
 */
std::tuple<State, OrderSubmitted> submit(State state, const Submission& submission);

/**
 * @brief Transmit every player's selection whose opportunity has come
 *
 * Each such selection is sent irrevocably, keeping its own submission step and the selection it replaced, and stops
 * being the player's selection; players without a selection transmit nothing, so an order is transmitted once. A
 * selection whose submission is sent at a later opportunity (Transmission Schedule Policy), such as one submitted in
 * the transmission's own step, stays selected.
 *
 * Admitted input: the step of a transmission opportunity.
 *
 * @param state Current selection state
 * @param at Step of the transmission
 * @return Tuple of (updated state, transmission records ordered by player)
 */
std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at);

/**
 * @brief End every selection without transmitting it
 *
 * Numbering continues: a later submission gets the player's next number.
 *
 * @param state Current selection state
 * @return Selection state without any selection
 */
State cancel(State state);

// ============================================================================
// Queries: read the state without evolving it
// ============================================================================

/**
 * @brief Projection of the selection state for presentation
 */
struct View {
  std::map<PlayerId, Selection> selections;  // Every player's selection, if they have one
};

/**
 * @brief Project the selection state
 *
 * @param state Current selection state
 * @return Each player's selection
 */
View view(const State& state);

}  // namespace order_selection_authority
}  // namespace orbital
}  // namespace snake
