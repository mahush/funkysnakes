#pragma once

#include <chrono>
#include <optional>

#include "common/random_source.hpp"
#include "orbital/arena_authority.hpp"
#include "orbital/forwarded_order_facts.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/order_replica_authority.hpp"
#include "orbital_loopback/link.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Host composition - the canonical composition of the Domain Application, with the players' orders received
 *
 * Platform code without meaning, free of transport and serialization so tests drive it directly. It composes the
 * Arena and Game Authorities as the Domain Application does; in place of the Order Authority, which runs on each
 * player's device, it keeps the Order Replica Authority, fed with the forwarded facts. Each step runs:
 *   orders transmitted for the step (Order Replica) → step (Arena) → interpret outcomes (Order Replica)
 *   → score, advance handover chains and check end (Game)
 *   → if concluded: end execution (Arena) → interpret endings and cancel (Order Replica)
 * A forwarded transmission is learned by the Game Authority when it arrives, with its own transmission step. The
 * views are the Domain Application's (orbital_game_domain_application::viewsOf) with the replica's order view.
 *
 * Step game time is logical: step k is at k × the step interval after the start. Whoever runs the host decides
 * when a step runs (a real-time loop, or a simulated clock in tests) and calls runStep.
 */
class HostComposition {
 public:
  /**
   * @param outbound Where the host reports; must outlive the composition
   * @param random_int_fn Random number source the Arena Authority draws from
   */
  HostComposition(HostOutbound& outbound, RandomIntGeneratorFn random_int_fn);

  /**
   * @brief Start the round with the Board Setup Policy's start layout, if the Admission Policy admits a start
   *
   * @param round Identity of the round
   * @param t0 Host game clock at the start, reported to the devices
   * @return Whether the round started
   */
  bool start(RoundId round, std::chrono::milliseconds t0);

  // Forwarded facts from a player's device; ignored before the start and after the conclusion
  void receive(const platform::ForwardedSubmission& submission);
  void receive(const platform::ForwardedTransmission& transmission);

  /**
   * @brief Run the next arena step; nothing before the start or after the conclusion
   */
  void runStep();

  bool isRunning() const;

 private:
  struct Game {
    arena_authority::State arena;
    platform::order_replica_authority::State orders;
    game_authority::State game;
  };

  void reportViews();

  HostOutbound& outbound_;
  RandomIntGeneratorFn random_int_fn_;
  std::optional<Game> game_;
};

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
