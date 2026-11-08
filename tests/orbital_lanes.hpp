#pragma once

#include <cstddef>

#include "common/random_source.hpp"
#include "orbital/arena_authority.hpp"
#include "orbital/food_policy.hpp"

namespace snake {
namespace orbital {

/**
 * @brief Random source that always picks the far corner of the board for new food
 */
inline RandomIntGeneratorFn makeCornerRandom() {
  return [](int /* min */, int max) { return max; };
}

/**
 * @brief Round 1 on board 20x10: A in row 2 heading right, B in row 7 heading right, the given food
 *
 * Filler food in row 9 brings the food to the minimum without random draws.
 */
inline arena_authority::RoundSetup makeLanes(FoodItems food) {
  for (int x = 15; food.size() < static_cast<std::size_t>(food_policy::MIN_FOOD_COUNT); ++x) {
    food.push_back(Point{x, 9});
  }
  return arena_authority::RoundSetup{RoundId{1},
                                     StartLayout{Board{20, 10},
                                                 {{PLAYER_A, SnakeSetup{Point{2, 2}, Direction::RIGHT, 3}},
                                                  {PLAYER_B, SnakeSetup{Point{2, 7}, Direction::RIGHT, 3}}}},
                                     food};
}

}  // namespace orbital
}  // namespace snake
