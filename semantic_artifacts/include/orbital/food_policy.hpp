#pragma once

namespace snake {
namespace orbital {
namespace food_policy {

/**
 * Orbital Food Policy - decides the food supply of an Orbital Orders round
 *
 * Stateless decision on how much food the board holds, from round start through every step. It places nothing; the
 * Arena tops the food up to this amount.
 */

// Minimum number of food items on the board
constexpr int MIN_FOOD_COUNT = 5;

}  // namespace food_policy
}  // namespace orbital
}  // namespace snake
