#pragma once

#include "snake/game_types.hpp"

namespace snake {

/**
 * @brief Check if snake's head collides with its own tail
 *
 * @param snake Snake to check
 * @return true if head is in tail (self-bite)
 */
bool snakeBitesItself(const Snake& snake);

/**
 * @brief Check if first snake's head bites second snake's body
 *
 * Returns true if the first snake's head is contained anywhere in the
 * second snake's body (head or tail).
 *
 * @param first Snake that might be biting
 * @param second Snake that might be bitten
 * @return true if first bites second
 */
bool firstBitesSecond(const Snake& first, const Snake& second);

/**
 * @brief Check if both snakes bite each other simultaneously
 *
 * @param a First snake
 * @param b Second snake
 * @return true if both bite each other
 */
bool bothBiteEachOther(const Snake& a, const Snake& b);

}  // namespace snake
