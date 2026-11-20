#pragma once

#include <chrono>
#include <functional>
#include <variant>
#include <vector>

#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/arena_events.hpp"
#include "orbital/forwarded_order_facts.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital_loopback/history.hpp"
#include "orbital_loopback/link.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Loopback realization - the host and one device per player in one process, without a network
 *
 * Platform code without meaning. A simulated clock drives everything in game time from the round start: the
 * host runs step k at k × the step interval; each device fires the opportunities of the schedule the host sent
 * at their times on its own timer; the players submit as scripted. At equal times the host's step comes first,
 * then the messages due, then the devices' timers, then the players: step k runs, the opportunity at k transmits,
 * then the submissions of step k follow. Device clocks are aligned exactly.
 *
 * The in-process transport keeps each link's messages in order, like a WebSocket; a delay hook holds any message
 * back to provoke races and late delivery.
 */

/**
 * @brief One message on a link between the host and a player's device
 */
struct LinkMessage {
  enum class Direction { UPLINK, DOWNLINK };

  Direction direction;
  PlayerId player;  // The device at the other end
  std::variant<platform::ForwardedSubmission,
               platform::ForwardedTransmission,
               RoundStarted,
               game_boundary::StepReport,
               ArenaEvents,
               game_authority::GameConcluded>
      message;
};

/**
 * @brief How long a message takes on its link; a link still delivers its messages in order
 */
using DelayFn = std::function<std::chrono::milliseconds(const LinkMessage& message)>;

/**
 * @brief A player's submission at a time after the round start
 */
struct ScriptedSubmission {
  std::chrono::milliseconds at;
  PlayerId player;
  Point target;
};

/**
 * @brief A game to play over loopback
 */
struct LoopbackGame {
  RoundId round{1};
  unsigned seed{0};
  std::vector<ScriptedSubmission> script;
  DelayFn delay;  // Empty: every message arrives at once
};

/**
 * @brief A seeded script of both players' submissions over a whole game
 *
 * Targets are drawn from the board and one row and column around it, so some are off the board; times are drawn
 * with gaps up to max_gap, including times on step boundaries and quick re-choices.
 *
 * @param seed Seed of the script
 * @param max_gap Longest gap between two submissions of a player
 */
std::vector<ScriptedSubmission> randomScript(unsigned seed, std::chrono::milliseconds max_gap);

/**
 * @brief Play a whole game over loopback, up to its conclusion
 *
 * @param game Round, seed, the players' submissions and the delay hook
 * @return What the host recorded
 */
HostRecord playOverLoopback(const LoopbackGame& game);

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
