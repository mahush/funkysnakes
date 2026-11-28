#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <variant>
#include <vector>

#include "domain/common/players.hpp"
#include "domain/common/random_source.hpp"
#include "domain/orbital/arena_authority.hpp"
#include "domain/orbital/game_authority.hpp"
#include "domain/orbital/game_boundary.hpp"
#include "domain/orbital/order.hpp"
#include "domain/orbital/order_selection_authority.hpp"
#include "orbital_loopback/placement_link.hpp"
#include "platform/orbital/transmission_reception_authority.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * @brief What became of a transmission that reached the host
 */
struct TransmissionReceipt {
  OrderTransmitted transmitted;
  platform::transmission_reception_authority::Receipt receipt;
  // Handovers the Game Authority counted on learning it, which it does only for a received transmission; none under the
  // admitted input, where a transmission is learned before the step that activates its order, so its handovers come
  // with that step's report
  std::vector<game_authority::HandoverGranted> handovers;

  bool operator==(const TransmissionReceipt& other) const noexcept {
    return transmitted == other.transmitted && receipt == other.receipt && handovers == other.handovers;
  }
};

/**
 * @brief What became of a submission record that reached the host
 */
struct SubmissionReceipt {
  OrderSubmitted submitted;
  platform::transmission_reception_authority::Receipt receipt;

  bool operator==(const SubmissionReceipt& other) const noexcept {
    return submitted == other.submitted && receipt == other.receipt;
  }
};

/**
 * @brief What the host recorded beside its reports, each list in order of occurrence
 */
struct Record {
  std::vector<SubmissionReceipt> submission_receipts;
  std::vector<TransmissionReceipt> receipts;
  std::vector<platform::transmission_reception_authority::LateTransmission> late;
  std::vector<platform::transmission_reception_authority::SupersededTransmission> superseded;
  std::vector<platform::transmission_reception_authority::DiscardedTransmission> discarded;
};

/**
 * Host composition - the host of the distributed Orbital realization: Arena, Game and Transmission Reception
 * Authorities and a selection view projected from the players' devices, with a composition of its own
 *
 * Platform code without meaning, free of clocks and transport: whoever runs the host supplies the host time and the
 * randomness and delivers the messages; each step runs once the host time reaches it, as the Pacing Policy times the
 * steps from the start. It routes the owners' facts as the Domain
 * Application does, but shares no code with it. Of the Domain System Boundary it reports what the realization places on
 * the host: start refusals, steps, orders ended with the round, the conclusion and the views; submissions and their
 * outcomes are the devices'. A step report carries no transmissions: they reach the host after their step and are
 * recorded on arrival as receipts.
 *
 * - start: as the Admission Policy admits it, the owners start their round and every device learns that the game
 *   started.
 * - A forwarded record takes effect as the Transmission Reception Authority receives it; its receipt is recorded.
 * - A received submission record shows the order in the selection view.
 * - A received transmission ends showing the player's selection, waits for its step (Transmission Reception
 *   Authority) and is learned (Game Authority).
 * - Each step: the orders that take part (Transmission Reception Authority) → step (Arena Authority) → score, advance
 *   handover chains and check end (Game Authority) → if concluded: end execution (Arena Authority) → clear the
 *   selection view → discard what still waits (Transmission Reception Authority); late, superseded and discarded
 *   transmissions are recorded. After the conclusion's report, every device learns of it.
 *
 * Every call reports what it caused, in order of occurrence, followed by the views once the game started. A call made
 * while the host is reporting, from within a callback of the players' side or of a device, is the next input: it takes
 * effect once the current report is complete. The callbacks must not throw, and under the admitted input no owner
 * throws: an exception is a bug, and the host is not to be used after it.
 *
 * Admitted input: the devices' records as the Transmission Reception Authority admits them, and once the game started
 * only of the round the host started; a host plays one round.
 */
class HostComposition : public placement_link::PhoneToHost {
 public:
  /**
   * @param players The players' side of what the host reports; must outlive the composition, its callbacks must not
   *                throw
   * @param phones The players' devices, told of the start and the conclusion; must outlive the composition, their
   *               callbacks must not throw
   * @param random_int_fn Random number source the owners draw from
   * @throws std::invalid_argument if the random number source is empty
   */
  HostComposition(game_boundary::PlayersEndOutbound& players,
                  std::vector<std::reference_wrapper<placement_link::HostToPhone>> phones,
                  RandomIntGeneratorFn random_int_fn);

  // One game reporting to one players' side: neither copied nor moved
  HostComposition(const HostComposition&) = delete;
  HostComposition& operator=(const HostComposition&) = delete;
  HostComposition(HostComposition&&) = delete;
  HostComposition& operator=(HostComposition&&) = delete;

  /**
   * @brief Begin the game with the start layout of the Board Setup Policy
   *
   * Admitted as the Admission Policy decides: only before a game has started. A refused start leaves the game
   * unchanged, is reported as StartRejected with the views of the game that already started, and no device is told.
   *
   * @param start The start interaction
   * @param t0 Host game clock when the game started; the steps are timed from it
   */
  void start(const game_boundary::Start& start, std::chrono::milliseconds t0);

  /**
   * @brief Run every step that is due by the given host time
   *
   * Runs, in step order, every step that has not run yet and whose time (Pacing Policy, from t0) the host time has
   * reached. Carries out the Game Authority's conclusion (see game_authority::GameConcluded): no step runs after the
   * step that concluded the game. A time by which no further step is due, also one earlier than before, runs no step.
   * Before a game has started nothing changes and nothing is reported.
   *
   * @param host_time Host game clock, the clock t0 is read from
   */
  void runDueSteps(std::chrono::milliseconds host_time);

  /**
   * @brief A device forwarded its Order Selection Authority's submission record
   *
   * Shown in the selection view as far as the Transmission Reception Authority receives it; its receipt is recorded.
   */
  void forward(const OrderSubmitted& submitted) override;

  /**
   * @brief A device forwarded its Order Selection Authority's transmission record
   *
   * Takes effect as far as the Transmission Reception Authority receives it; its receipt is recorded.
   */
  void forward(const OrderTransmitted& transmitted) override;

  /**
   * @brief What the host recorded so far
   */
  const Record& record() const;

  /**
   * @brief State of the host composition
   */
  struct State {
    std::optional<arena_authority::State> arena;  // Empty until a game started
    std::optional<game_authority::State> game;
    platform::transmission_reception_authority::State reception{platform::transmission_reception_authority::initial()};
    order_selection_authority::View selections;  // Projected from the devices' records
    std::chrono::milliseconds t0{0};             // Host game clock when the game started
    Record record;
  };

 private:
  // A start with its time, as the host takes it
  struct StartAt {
    game_boundary::Start start;
    std::chrono::milliseconds t0;
  };

  // The steps due by a host time are to run
  struct DueStepsAt {
    std::chrono::milliseconds host_time;
  };

  // An input waiting to take effect
  using Input = std::variant<StartAt, DueStepsAt, OrderSubmitted, OrderTransmitted>;

  // Let the input take effect and report it, after every earlier input has been reported
  void take(Input input);

  game_boundary::PlayersEndOutbound& players_;
  std::vector<std::reference_wrapper<placement_link::HostToPhone>> phones_;
  RandomIntGeneratorFn random_int_fn_;
  State state_;
  std::deque<Input> waiting_;  // Inputs that arrived during a report, oldest first
  bool reporting_{false};      // Whether inputs are being taken and reported
};

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
