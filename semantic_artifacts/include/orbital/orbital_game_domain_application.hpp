#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include "common/game_time.hpp"
#include "common/random_source.hpp"
#include "orbital/arena_authority.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital/order_authority.hpp"
#include "orbital/pacing_policy.hpp"

namespace snake {
namespace orbital {
namespace orbital_game_domain_application {

// The players' interactions, from the Domain System Boundary
using game_boundary::Start;
using game_boundary::Submit;

/**
 * Orbital Game Domain Application - canonical single-process realization of Orbital Orders
 *
 * Composes the Arena, Order and Game Authorities and their Policies without actors, timers,
 * networking or rendering. The owners hold all of the game's meaning and stay pure; the Domain
 * Application keeps the game's state, routes interactions and Authority outputs mechanically through
 * pure composition, and makes no gameplay decision of its own. Its only effects are calls of the
 * players' outbound interface, which its caller supplies, so a run stays deterministic.
 *
 * It has two interfaces. The Domain System Boundary carries the players' interactions: it implements
 * game_boundary::PlayersEndInbound and reports through game_boundary::PlayersEndOutbound. The
 * ExecutionInterface carries what no counterpart controls and whoever runs the Domain Application
 * supplies: randomness, given at construction, and time passing. A test simulates both; a front end
 * measures real time and seeds an engine. A third execution input is implicit: interactions take
 * effect one at a time, in the order of the calls, so the caller fixes their delivery order.
 * Observations caused by execution inputs, such as step reports, the conclusion and the views, still
 * go out through the boundary.
 *
 * One Domain Application plays one game. Which interaction it admits in which phase of the game (not
 * started, running, concluded) is the Admission Policy's answer, which it passes on as the outcome.
 *
 * One logical clock drives the round; steps fall due at the Pacing Policy's step interval. A
 * submission is committed (Order Authority) at the clock's current time and its commitment is passed on
 * to the Game Authority. Each step that falls due runs:
 *   pending orders (Order Authority) released by the Order Release Policy → step (Arena Authority)
 *   → interpret outcomes (Order Authority)
 *   → score, advance handover chains and check end (Game Authority)
 *   → if concluded: end execution (Arena Authority) → interpret endings (Order Authority)
 *   → cancel pending (Order Authority)
 * The clock never stands on a step that has not run, so every commitment lies strictly before the
 * next step. A commitment can share the time of the last step that ran; it was still made after
 * that step, which the Game Authority's rule for handovers (committed strictly before the completing step)
 * reflects.
 */

// ============================================================================
// Execution interface - supplied by whoever runs the Domain Application
// ============================================================================

/**
 * @brief Game time has passed
 */
struct TimeElapsed {
  std::chrono::milliseconds duration;  // Not negative
};

/**
 * @brief Execution interface: what whoever runs the game supplies
 *
 * Randomness is given at construction and drawn from whenever an owner needs it, at the start as
 * well as during steps; time passing is supplied through elapse. Unlike a pure interface, it holds
 * the random source: that the game cannot exist without one is part of the contract.
 */
class ExecutionInterface {
 public:
  virtual ~ExecutionInterface() = default;

  /**
   * @brief Let game time pass
   *
   * @param elapsed Elapsed game time, not negative
   * @throws std::invalid_argument if the elapsed time is negative
   */
  virtual void elapse(const TimeElapsed& elapsed) = 0;

 protected:
  /**
   * @param random_int_fn Random number source the owners draw from
   * @throws std::invalid_argument if the random number source is empty
   */
  explicit ExecutionInterface(RandomIntGeneratorFn random_int_fn) : random_int_fn_{std::move(random_int_fn)} {
    if (!random_int_fn_) {
      throw std::invalid_argument{"a random number source is required"};
    }
  }

  /**
   * @brief Random number source given at construction
   */
  const RandomIntGeneratorFn& randomIntFn() const { return random_int_fn_; }

 private:
  RandomIntGeneratorFn random_int_fn_;
};

// ============================================================================
// Domain Application
// ============================================================================

/**
 * @brief Virtual clock of the round
 *
 * Mechanics only: it accumulates elapsed game time and reports when steps fall due.
 */
struct VirtualClock {
  GameTime now{0};
  GameTime next_step{pacing_policy::STEP_INTERVAL};
};

/**
 * @brief State of the Domain Application
 */
struct State {
  std::optional<arena_authority::State> arena;  // Empty until a game started
  std::optional<order_authority::State> orders;
  std::optional<game_authority::State> game;
  VirtualClock clock;
};

/**
 * @brief The Orbital Orders game in one process, before any game has started
 *
 * Every call reports what it caused to the players' outbound interface before it returns, in order
 * of occurrence. A call made while the game is reporting, from within an outbound callback, is the
 * next interaction: it takes effect once the current report is complete, so a report never mixes
 * two interactions and the last views reported are always the current ones. If a callback throws,
 * the exception propagates: the input being reported has taken effect, the rest of its report and
 * the calls still waiting are dropped, and later calls are taken as usual. If an owner throws while
 * an input takes effect, such as a random source that fails, that input takes no effect and nothing
 * of it is reported.
 */
class DomainApplication : public game_boundary::PlayersEndInbound, public ExecutionInterface {
 public:
  /**
   * @param players The players' side; must outlive the Domain Application
   * @param random_int_fn Random number source the owners draw from
   */
  DomainApplication(game_boundary::PlayersEndOutbound& players, RandomIntGeneratorFn random_int_fn);

  // One game reporting to one players' side: neither copied nor moved
  DomainApplication(const DomainApplication&) = delete;
  DomainApplication& operator=(const DomainApplication&) = delete;
  DomainApplication(DomainApplication&&) = delete;
  DomainApplication& operator=(DomainApplication&&) = delete;

  /**
   * @brief Begin the game with the start layout of the Board Setup Policy
   *
   * Admitted as the Admission Policy decides: only before a game has started. A refused start
   * leaves the game unchanged and is reported as StartRejected, with the views of the game that
   * already started.
   */
  void start(const Start& start) override;

  /**
   * @brief Submit an order
   *
   * Admitted as the Admission Policy decides: only while the game is running; otherwise it is
   * rejected with the Policy's reason and the game is unchanged.
   */
  void submit(const Submit& submit) override;

  /**
   * @brief Let game time pass: run every arena step that falls due
   *
   * Carries out the Game Authority's conclusion (see game_authority::GameConcluded): the clock stops
   * at the step that concluded the game, and time passing afterwards changes nothing. Before a game
   * has started there is no game time: nothing changes and nothing is reported. A negative elapsed
   * time is refused before anything changes.
   */
  void elapse(const TimeElapsed& elapsed) override;

  /**
   * @brief Begin the game with a given world setup instead of the Board Setup Policy's start layout
   *
   * Test entry, part of neither interface: it lets a history start from a hand-picked world.
   * Admitted like start: only before a game has started.
   *
   * @param setup World at round start; its round identity names the game
   */
  void startWithSetup(const arena_authority::RoundSetup& setup);

 private:
  // An interaction, an execution input or a start with a given setup, waiting to take effect
  using Input = std::variant<Start, Submit, TimeElapsed, arena_authority::RoundSetup>;

  // Let the input take effect and report it, after every earlier input has been reported
  void take(Input input);

  game_boundary::PlayersEndOutbound& players_;
  State state_;
  std::deque<Input> waiting_;  // Inputs that arrived during a report, oldest first
  bool reporting_{false};      // Whether inputs are being taken and reported
};

}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
