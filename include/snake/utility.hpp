#ifndef SNAKE_UTILITY_HPP
#define SNAKE_UTILITY_HPP

#include <functional>

namespace snake {

// Random integer generator function type
using RandomIntGeneratorFn = std::function<int(int, int)>;

/**
 * @brief Create a random integer generator function
 *
 * Returns a function that generates random integers in a given range [min, max].
 * Uses static random_device and mt19937 for thread-safe randomness.
 *
 * @return Function that takes (min, max) and returns random int in that range
 */
RandomIntGeneratorFn makeRandomIntGenerator();

}  // namespace snake

#endif  // SNAKE_UTILITY_HPP
