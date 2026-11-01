#pragma once

#include <variant>
#include <vector>

#include "common/players.hpp"
#include "orbital/arena_events.hpp"
#include "orbital/identities.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {
namespace scoring_policy {

/**
 * Orbital Scoring Policy - decides what arena events are worth and when points count
 *
 * Stateless decision: it names point effects and settlements; holding and applying them to a
 * score is up to its user. Food and collision values follow classic snake, except that food
 * without an order is worth less: eating during an order risks its points, so it has to be worth
 * more than eating while driving straight. The timing of when points count is Orbital's own rule:
 * - FoodCollected during an order: +10 held for that order
 * - FoodCollected without an order: +5 banked at once
 * - Bitten: the victim loses 10 banked at once, the biter gets nothing
 * - SelfBitten: the snake loses 10 banked at once
 * - MutualBite: both snakes lose 10 banked at once
 * - An order ending COMPLETED: the order's held points are banked
 * - An order ending INTERRUPTED, ELIMINATED or ROUND_ENDED: the order's held points are dropped
 *
 * Submitting or superseding a pending order is no arena event and changes no points. A
 * nonlethal bite leaves an order's held points intact.
 *
 * A completion that hands over to an already committed, still pending successor is worth a
 * handover bonus in addition; whether a completion qualifies is the Game Authority's chain. The
 * first handover of a chain is worth the base bonus, each consecutive one doubles it, up to a cap.
 * Base and cap are provisional tuning values.
 */

constexpr int FOOD_POINTS = 10;          // During an order, held
constexpr int FOOD_POINTS_NO_ORDER = 5;  // Without an order, banked at once
constexpr int COLLISION_PENALTY = 10;
constexpr int HANDOVER_BONUS_BASE = 10;  // First handover of a chain
constexpr int HANDOVER_BONUS_CAP = 80;   // Doubling stops here

/**
 * @brief Points that count at once; negative for a penalty
 */
struct Bank {
  PlayerId player;
  int points;

  bool operator==(const Bank& other) const noexcept { return player == other.player && points == other.points; }
};

/**
 * @brief Points that count only once the order they are held for is banked
 */
struct Hold {
  OrderId order;
  PlayerId player;
  int points;

  bool operator==(const Hold& other) const noexcept {
    return order == other.order && player == other.player && points == other.points;
  }
};

using PointEffect = std::variant<Bank, Hold>;

/**
 * @brief What happens to an ended order's held points
 */
enum class Settlement {
  BANK,  // Added to the player's banked points
  DROP   // Dropped
};

/**
 * @brief Point effects of one arena event
 *
 * An order ending carries no effect of its own; what it means for the order's held points
 * is decided by settlementOf.
 *
 * @param event Arena event
 * @return Point effects of the event, in order; empty if it is worth nothing
 */
std::vector<PointEffect> effectsOf(const ArenaEvent& event);

/**
 * @brief Settlement of an order's held points when the order ends
 *
 * @param reason Why the order stopped executing
 * @return BANK for a completed order, DROP for any other ending
 */
Settlement settlementOf(EndReason reason);

/**
 * @brief Bonus for the given handover of a chain
 *
 * @param handover Position of the handover in its chain, starting at 1
 * @return Base bonus doubled for each earlier handover of the chain, at most the cap
 */
int handoverBonus(int handover);

}  // namespace scoring_policy
}  // namespace orbital
}  // namespace snake
