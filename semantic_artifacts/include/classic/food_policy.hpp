#pragma once

namespace snake {
namespace food_policy {

/**
 * Food Policy - decides the food supply of the classic arena
 *
 * Stateless decision on how much food the board holds, from the start through every step. It places nothing; the
 * arena tops the food up to this amount.
 */

// Minimum number of food items on the board
constexpr int MIN_FOOD_COUNT = 5;

}  // namespace food_policy
}  // namespace snake
