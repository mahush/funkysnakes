#include "snake/snake_predicates.hpp"

#include <algorithm>

namespace snake {

bool snakeBitesItself(const Snake& snake) {
  return std::find(snake.tail().begin(), snake.tail().end(), snake.head()) != snake.tail().end();
}

bool firstBitesSecond(const Snake& first, const Snake& second) {
  const Point& attacker_head = first.head();

  if (attacker_head == second.head()) {
    return true;
  }

  return std::find(second.tail().begin(), second.tail().end(), attacker_head) != second.tail().end();
}

bool bothBiteEachOther(const Snake& a, const Snake& b) { return firstBitesSecond(a, b) && firstBitesSecond(b, a); }

}  // namespace snake
