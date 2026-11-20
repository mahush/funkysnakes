#pragma once

#include <functional>

namespace snake {

/**
 * @brief Random source supplied by the realization as an external fact
 *
 * Returns a random integer in the range [min, max].
 */
using RandomIntGeneratorFn = std::function<int(int, int)>;

}  // namespace snake
