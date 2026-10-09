#pragma once

#include <variant>
#include <vector>

#include "common/players.hpp"

namespace snake {

/**
 * Arena events shared by the game variants - what the shared arena rules report
 *
 * Arena rules report these instead of deciding their consequences (such as scores).
 * Each event kind is a deliberate distinction of the arena's vocabulary. Each game variant
 * collects the events of its arena step in its own event list.
 */

// A snake's head reached a food item and ate it
struct FoodEaten {
  PlayerId player;

  bool operator==(const FoodEaten& other) const noexcept { return player == other.player; }
};

// A snake's head reached another snake's body; the victim was cut at that point
struct Bitten {
  PlayerId victim;
  PlayerId biter;

  bool operator==(const Bitten& other) const noexcept { return victim == other.victim && biter == other.biter; }
};

// A snake's head reached its own tail; the snake died
struct SelfBitten {
  PlayerId player;

  bool operator==(const SelfBitten& other) const noexcept { return player == other.player; }
};

// Two snakes bit each other in the same step (head-on or mutual tail bites); both died
struct MutualBite {
  PlayerId first;
  PlayerId second;

  bool operator==(const MutualBite& other) const noexcept { return first == other.first && second == other.second; }
};

// What collision handling reports
using CollisionEvent = std::variant<Bitten, SelfBitten, MutualBite>;
using CollisionEvents = std::vector<CollisionEvent>;

}  // namespace snake
