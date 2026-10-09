#include "snake/difficulty_policy.hpp"

#include <algorithm>

namespace snake {
namespace difficulty_policy {

int stepIntervalMs(int level) {
  return std::max(MIN_INTERVAL_MS, BASE_INTERVAL_MS - ((level - 1) * REDUCTION_PER_LEVEL_MS));
}

}  // namespace difficulty_policy
}  // namespace snake
