#pragma once

#include <chrono>

#include "domain/orbital/game_authority.hpp"
#include "domain/orbital/identities.hpp"
#include "domain/orbital/order.hpp"

namespace snake {
namespace orbital {
namespace loopback {
namespace placement_link {

/**
 * Placement link between the host and a player's device of the distributed Orbital realization
 *
 * A placement link connects two placements of one realization. It carries the owners' facts that the canonical
 * composition passes directly: the device forwards what its player's Order Selection Authority published, and the host
 * tells the device that the game started and that the Game Authority concluded it. It is not a system boundary: both
 * ends belong to the same Domain System, and the players reach the game through the Domain System Boundary only.
 *
 * Platform code without meaning. Every link delivers its messages as their receivers admit them; for the host that is
 * the Transmission Reception Authority's admitted input. A transport that resends, for instance after a reconnect, does
 * not do that by itself.
 */

/**
 * @brief The host started the game
 */
struct GameStarted {
  RoundId round;
  std::chrono::milliseconds t0;  // Host game clock when the game started
};

/**
 * @brief What a player's device sends to the host; implemented by the host
 */
class PhoneToHost {
 public:
  virtual ~PhoneToHost() = default;

  /**
   * @brief A submission record of the device's Order Selection Authority
   */
  virtual void forward(const OrderSubmitted& submitted) = 0;

  /**
   * @brief A transmission record of the device's Order Selection Authority
   */
  virtual void forward(const OrderTransmitted& transmitted) = 0;
};

/**
 * @brief What the host sends to a player's device; implemented by the device
 */
class HostToPhone {
 public:
  virtual ~HostToPhone() = default;

  /**
   * @brief The host started the game
   */
  virtual void gameStarted(const GameStarted& started) = 0;

  /**
   * @brief The Game Authority concluded the game
   */
  virtual void concluded(const game_authority::GameConcluded& concluded) = 0;
};

}  // namespace placement_link
}  // namespace loopback
}  // namespace orbital
}  // namespace snake
