#pragma once

#include <variant>
#include <vector>

#include "classic/players.hpp"

namespace snake {

/**
 * Arena events - facts about what happened during an arena step
 *
 * Arena rules report these instead of deciding their consequences (such as scores).
 * Each event kind is a deliberate distinction of the arena's vocabulary.
 */

// A snake's head reached a food item and ate it
struct FoodEaten {
  PlayerId player;
};

// A snake's head reached another snake's body; the victim was cut at that point
struct Bitten {
  PlayerId victim;
  PlayerId biter;
};

// A snake's head reached its own tail; the snake died
struct SelfBitten {
  PlayerId player;
};

// Two snakes bit each other in the same step (head-on or mutual tail bites); both died
struct MutualBite {
  PlayerId first;
  PlayerId second;
};

using ArenaEvent = std::variant<FoodEaten, Bitten, SelfBitten, MutualBite>;
using ArenaEvents = std::vector<ArenaEvent>;

}  // namespace snake
